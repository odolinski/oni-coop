#include "gauge.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "log.h"
#include "memory.h"

using namespace oni;

namespace gauge {

// Le moteur sait tracer un rectangle texturé face à la caméra : c'est ce qu'il fait du nom d'un
// personnage dans son affichage de mise au point, en noir sur une texture blanche. Ici la barre
// est une texture unie que l'on teinte, et chaque chiffre a la sienne, transparente autour du
// tracé : rien ne dépend plus du texte du moteur ni de son fond.
static const int kGlyphWidth = 16;
static const int kGlyphHeight = 32;
static const int kGlyphCount = 11;             // 0 à 9, puis la barre oblique
static const int kSlash = 10;
static const int kSolidSize = 8;

static const float kHeight = 8.5f;             // au-dessus du point d'ancrage du moteur
static const float kHalfWidth = 3.0f;          // demi-largeur de la jauge de mise au point du moteur
static const float kHalfHeight = 0.6f;
static const float kBorder = 0.14f;
static const float kDigitHalfHeight = 0.5f;
static const float kDigitHalfWidth = kDigitHalfHeight * kGlyphWidth / kGlyphHeight;
static const float kDigitAdvance = 1.5f;       // d'un chiffre au suivant, en demi-largeurs de chiffre

static const uint32_t kBorderColor = 0xFF000000;
static const uint32_t kEmptyColor = 0xFF303030;

// Étiquettes : une lettre par case de 16 x 16 points, tirée d'une police de 5 x 7.
static const int kLetterSize = 16;
static const int kLetterCount = 40;            // 0 à 9, A à Z, « _ », « - », « . », « ? »
static const int kLetterUnknown = 39;
static const float kLetterHalf = 0.42f;        // demi-côté d'une case, en unités du monde
static const float kLetterAdvance = 1.5f;      // d'une lettre à la suivante, en demi-côtés
static const int kMaxLabel = 24;
static const uint32_t kPlateColor = 0xFF000000;
static const uint32_t kPlateAlpha = 150;

// Réserves d'un joueur : une seule rangée de six cases carrées au-dessus de sa jauge, petite et
// centrée. Les chargeurs de balles la garnissent en rouge depuis la gauche, les cellules d'énergie
// en vert depuis la droite.
static const int kReserveSlots = 12;           // ce qu'un joueur peut porter de chaque (six dans le jeu d'origine)
static const float kSlotHalf = 0.2f;           // demi-côté d'une case, en unités du monde
static const float kSlotSpacing = 0.5f;        // d'une case à la suivante : douze cases sur la largeur de la jauge
static const float kSlotInset = 0.78f;         // part de la case que couvre un carré de couleur
static const float kRowGap = 0.06f;            // entre la jauge et la rangée
static const float kRowPad = 0.08f;            // fond de la rangée autour des cases
static const uint32_t kSlotColor = 0xFF242424; // case vide
static const uint32_t kRowAlpha = 150;

// Jauges des joueurs lisibles à travers les murs : tracées à cette distance de la caméra, sur la
// ligne qui mène au joueur, et réduites d'autant. Devant presque tout le décor, donc par-dessus.
static const float kOverlayDistance = 10.0f;
static const float kOverlayCone = 0.2f;        // cosinus : pas de jauge pour un joueur derrière la caméra
static bool g_overlay = false;

// Panneau d'un joueur à terre, aux couleurs des écrans du jeu.
static const uint32_t kPanelColor = 0xFF081424;    // bleu nuit
static const uint32_t kPanelAlpha = 215;
static const uint32_t kFrameColor = 0xFF6CB4E8;    // filet bleu clair
static const uint32_t kBleedColor = 0xFFFFA81E;    // ambre : il reste du temps
static const uint32_t kBleedLowColor = 0xFFE62828; // rouge : presque plus
static const uint32_t kProgressColor = 0xFF8CE6FF; // bleu clair : la réanimation avance
static const float kBleedLow = 0.3f;               // sous cette part du temps, la barre bat
static const float kDownedScale = 1.3f;            // un joueur à terre doit se voir de loin
static const float kBeaconRange = 90.0f;           // au-delà, le panneau garde sa taille à l'écran
static const float kBeaconMax = 7.0f;              // jusqu'à sept fois cette distance
static const int kBarTicks = 10;                   // graduations de la barre
static const float kTickHalf = 0.035f;
static const float kHintScale = 0.55f;             // l'invite, sous la barre, en plus petit
static const int kIconSize = 32;
static const float kIconHalf = 0.95f;
static void* g_icon = nullptr;                     // nul : pas de repère dans ce niveau

static void* g_solid = nullptr;                // nul : textures à créer
static void* g_glyphs[kGlyphCount] = {};
static void* g_letters[kLetterCount] = {};
static bool g_lettersReady = false;            // les jauges peuvent exister sans les étiquettes
static uint32_t g_level = 0;                   // niveau et instant de leur création
static int32_t g_time = 0;
static bool g_unavailable = false;             // création refusée dans ce niveau

// -------------------------------------------------------------------------------------------------
// Textures
// -------------------------------------------------------------------------------------------------
static uint16_t* NewTexture(int width, int height, const char* name, void*& texture) {
    texture = nullptr;
    void* created = nullptr;
    if (fn::TextureNew(static_cast<uint32_t>(width), static_cast<uint32_t>(height), off::kTexelArgb4444,
                       fn::kTextureAllocate, 0, name, &created) != 0 || !created) {
        return nullptr;
    }
    // On écrit directement dans ses points : elle doit avoir la forme demandée.
    uint16_t* pixels = field<uint16_t*>(created, off::kTxPixels);
    if (!pixels || field<uint16_t>(created, off::kTxWidth) != width ||
        field<uint16_t>(created, off::kTxHeight) != height ||
        field<uint32_t>(created, off::kTxTexelType) != off::kTexelArgb4444) {
        return nullptr;
    }
    texture = created;
    return pixels;
}

// Point d'opacité alpha, du noir (white = 0) au blanc (white = 1).
static uint16_t Texel(float alpha, float white) {
    uint16_t a = static_cast<uint16_t>(alpha * 15.0f + 0.5f);
    uint16_t v = static_cast<uint16_t>(white * 15.0f + 0.5f);
    return static_cast<uint16_t>((a << 12) | (v << 8) | (v << 4) | v);
}

// Chiffres à sept segments, tracés en traits épais dans une case de 16 x 32 points.
struct Stroke {
    float x0, y0, x1, y1;
};
static const float kLeft = 4.5f, kRight = 11.5f, kTop = 5.5f, kMiddle = 16.0f, kBottom = 26.5f;
static const Stroke kSegments[7] = {
    {kLeft, kTop, kRight, kTop},         // haut
    {kRight, kTop, kRight, kMiddle},     // haut droit
    {kRight, kMiddle, kRight, kBottom},  // bas droit
    {kLeft, kBottom, kRight, kBottom},   // bas
    {kLeft, kMiddle, kLeft, kBottom},    // bas gauche
    {kLeft, kTop, kLeft, kMiddle},       // haut gauche
    {kLeft, kMiddle, kRight, kMiddle},   // milieu
};
// Segments allumés pour chaque chiffre : bit 0 pour le premier de kSegments.
static const uint8_t kDigits[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
static const Stroke kSlashStroke = {kRight, kTop, kLeft, kBottom};
static const float kStrokeRadius = 1.2f;       // demi-épaisseur du trait blanc
static const float kOutline = 1.5f;            // liseré noir autour, pour rester lisible sur la barre

static float Distance(const Stroke& stroke, float x, float y) {
    float dx = stroke.x1 - stroke.x0, dy = stroke.y1 - stroke.y0;
    float t = ((x - stroke.x0) * dx + (y - stroke.y0) * dy) / (dx * dx + dy * dy);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float px = stroke.x0 + t * dx - x, py = stroke.y0 + t * dy - y;
    return std::sqrt(px * px + py * py);
}

static float Coverage(float distance, float radius) {
    float coverage = radius + 0.5f - distance;
    return coverage < 0.0f ? 0.0f : (coverage > 1.0f ? 1.0f : coverage);
}

static void DrawGlyph(uint16_t* pixels, int glyph) {
    for (int y = 0; y < kGlyphHeight; y++) {
        for (int x = 0; x < kGlyphWidth; x++) {
            float distance = 1000.0f;
            if (glyph == kSlash) {
                distance = Distance(kSlashStroke, x + 0.5f, y + 0.5f);
            } else {
                for (int segment = 0; segment < 7; segment++) {
                    if (!(kDigits[glyph] & (1 << segment))) continue;
                    float d = Distance(kSegments[segment], x + 0.5f, y + 0.5f);
                    if (d < distance) distance = d;
                }
            }
            pixels[y * kGlyphWidth + x] =
                Texel(Coverage(distance, kStrokeRadius + kOutline), Coverage(distance, kStrokeRadius));
        }
    }
}

// Police de 5 x 7 : sept rangées par signe, du haut vers le bas, le bit 4 à gauche.
static const uint8_t kFont[kLetterCount][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},  // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},  // 2
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},  // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},  // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},  // 9
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},  // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},  // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},  // C
    {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C},  // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},  // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},  // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},  // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},  // I
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},  // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},  // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},  // L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},  // M
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11},  // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},  // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},  // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},  // R
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},  // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},  // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},  // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},  // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},  // X
    {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04},  // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},  // Z
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},  // _
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},  // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},  // .
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},  // ?
};

// Signe de la police pour ce caractère ; négatif : une espace.
static int LetterOf(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + c - 'A';
    if (c >= 'a' && c <= 'z') return 10 + c - 'a';
    if (c == '_') return 36;
    if (c == '-') return 37;
    if (c == '.') return 38;
    return c == ' ' ? -1 : kLetterUnknown;
}

// Chaque point de la police vaut deux points de la case, qui garde une marge pour le liseré.
static bool Lit(int letter, int x, int y) {
    int column = x - 3, row = y - 1;
    if (column < 0 || column >= 10 || row < 0 || row >= 14) return false;
    return (kFont[letter][row / 2] & (0x10 >> (column / 2))) != 0;
}

static void DrawLetter(uint16_t* pixels, int letter) {
    for (int y = 0; y < kLetterSize; y++) {
        for (int x = 0; x < kLetterSize; x++) {
            bool lit = Lit(letter, x, y), edge = false;
            for (int dy = -1; dy <= 1 && !lit && !edge; dy++) {
                for (int dx = -1; dx <= 1; dx++) edge = edge || Lit(letter, x + dx, y + dy);
            }
            // Trait blanc, liseré noir, et rien autour.
            pixels[y * kLetterSize + x] = lit ? Texel(1.0f, 1.0f) : (edge ? Texel(1.0f, 0.0f) : Texel(0.0f, 0.0f));
        }
    }
}

// Repère d'un joueur à terre : une croix de secours dans un anneau, blanche avec son liseré noir,
// à teinter au tracé.
static void DrawIcon(uint16_t* pixels) {
    const float center = kIconSize * 0.5f;
    const Stroke across = {center - 6.0f, center, center + 6.0f, center};
    const Stroke upright = {center, center - 6.0f, center, center + 6.0f};
    for (int y = 0; y < kIconSize; y++) {
        for (int x = 0; x < kIconSize; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            // Distance au bord de la forme, négative dedans : l'anneau, ou la croix.
            float ring = std::fabs(std::sqrt((px - center) * (px - center) + (py - center) * (py - center)) - 12.5f) - 1.4f;
            float a = Distance(across, px, py), b = Distance(upright, px, py);
            float cross = (a < b ? a : b) - 2.3f;
            float distance = ring < cross ? ring : cross;
            pixels[y * kIconSize + x] = Texel(Coverage(distance, kOutline), Coverage(distance, 0.0f));
        }
    }
}

// Faux : pas d'étiquettes dans ce niveau. Les jauges n'en dépendent pas.
static bool BuildLetters() {
    for (int letter = 0; letter < kLetterCount; letter++) {
        char name[32];
        snprintf(name, sizeof(name), "onimp letter %d", letter);
        uint16_t* pixels = NewTexture(kLetterSize, kLetterSize, name, g_letters[letter]);
        if (!pixels) return false;
        DrawLetter(pixels, letter);
        if (fn::InstanceUpdate(g_letters[letter]) != 0) return false;
    }
    return true;
}

static bool Build() {
    if (!fn::TexelTypeAvailable(off::kTexelArgb4444)) return false;

    void* solid = nullptr;
    uint16_t* pixels = NewTexture(kSolidSize, kSolidSize, "onimp gauge", solid);
    if (!pixels) return false;
    for (int i = 0; i < kSolidSize * kSolidSize; i++) pixels[i] = 0xFFFF;
    if (fn::InstanceUpdate(solid) != 0) return false;

    for (int glyph = 0; glyph < kGlyphCount; glyph++) {
        char name[32];
        snprintf(name, sizeof(name), "onimp digit %d", glyph);
        pixels = NewTexture(kGlyphWidth, kGlyphHeight, name, g_glyphs[glyph]);
        if (!pixels) return false;
        DrawGlyph(pixels, glyph);
        if (fn::InstanceUpdate(g_glyphs[glyph]) != 0) return false;
    }
    g_solid = solid;
    g_lettersReady = BuildLetters();
    if (!g_lettersReady) LOG("jauge : textures des etiquettes refusees par le moteur, pas d'identifiants dans ce niveau");
    // Le repère d'un joueur à terre : sans lui, son panneau reste.
    void* icon = nullptr;
    pixels = NewTexture(kIconSize, kIconSize, "onimp downed", icon);
    if (pixels) {
        DrawIcon(pixels);
        if (fn::InstanceUpdate(icon) == 0) g_icon = icon;
    }
    return true;
}

void OnLevelChanged() {
    g_solid = nullptr;
    g_icon = nullptr;
    g_lettersReady = false;
    g_unavailable = false;
}

static bool Ready() {
    uint32_t level = mem::LevelHash();
    int32_t time = mem::GameTime();
    // Les textures sont libérées avec le niveau où elles ont été créées. OnLevelChanged le signale
    // à chaque chargement ; ceci couvre une image tracée avant qu'il ne passe.
    if (g_solid && (level != g_level || time < g_time)) OnLevelChanged();
    if (g_solid) return true;
    if (g_unavailable) return false;

    g_level = level;
    g_time = time;
    if (Build()) return true;
    g_unavailable = true;
    LOG("jauge : textures refusees par le moteur, pas de sante au-dessus des personnages dans ce niveau");
    return false;
}

// -------------------------------------------------------------------------------------------------
// Tracé
// -------------------------------------------------------------------------------------------------
uint32_t PlayerColor(int player) {
    static const uint32_t kColors[] = {0xFF2ECC40, 0xFF2E8BFF, 0xFFFF9500, 0xFFFFE433};  // vert, bleu, orange, jaune
    return player >= 0 && player < 4 ? kColors[player] : 0xFFFFFFFF;
}
// La jauge est un empilement de rectangles : cadre, barre vide, barre pleine, chiffres. Pendant le
// rendu de la scène, le moteur ne trace pas un rectangle transparent quand on le lui demande : il
// le garde pour la fin de l'image, dans une liste triée par profondeur. Les nôtres, tous au même
// point, y avaient la même, et sortaient du tri dans un ordre qui changeait d'une image à l'autre :
// le cadre ou la barre vide passait par moments devant le reste, et la jauge clignotait (versions
// 0.11 à 0.16). Ils sont donc tracés sur-le-champ, dans l'ordre des appels, chaque couche un peu
// plus près de la caméra que la précédente : elle passe devant quelle que soit la comparaison de
// profondeur, et la jauge, inscrite dans la profondeur, n'est pas recouverte par ce que le moteur
// trace derrière elle après les personnages (portes, objets, particules).
struct Stack {
    Vector3 base;       // centre de la première couche
    Vector3 toCamera;   // de là jusqu'à la caméra
    float step;         // part de ce trajet d'une couche à la suivante
    float scale;        // taille à donner aux couches : 1, ou moins pour une pile rapprochée de la caméra
};

// La profondeur perd en finesse avec la distance : le pas grandit avec elle.
static const float kMinLayerStep = 0.004f;
static const float kMaxLayerStep = 0.03f;
static const float kLayerStepRange = 40000.0f;  // distance / pas

// Rapprochée le long de la ligne de visée et réduite d'autant, une couche garde sa place et sa
// taille à l'écran.
static void Layer(const Stack& stack, int layer, void* texture, float halfWidth, float halfHeight, uint32_t color,
                  uint32_t alpha, float offset = 0.0f, float chop = 0.0f) {
    float closer = stack.step * static_cast<float>(layer);
    Vector3 center = {stack.base.x + stack.toCamera.x * closer, stack.base.y + stack.toCamera.y * closer,
                      stack.base.z + stack.toCamera.z * closer};
    float size = (1.0f - closer) * stack.scale;
    fn::SpriteDraw(texture, &center, halfWidth * size, halfHeight * size, color, alpha, 0.0f, nullptr, nullptr, offset,
                   0.0f, chop);
}

void SetOverlay(bool overlay) { g_overlay = overlay; }

// Pile de couches centrée sur base. Faux si la caméra est dedans, ou s'il n'y en a pas.
static bool MakeStack(const Vector3& base, Stack& stack) {
    void* camera = *reinterpret_cast<void**>(addr::kSceneCameraPtr);
    if (!camera) return false;
    stack.base = base;
    stack.scale = 1.0f;
    Vector3 eye, view = {};
    fn::CameraView(camera, &eye, &view, nullptr);
    stack.toCamera = {eye.x - stack.base.x, eye.y - stack.base.y, eye.z - stack.base.z};
    float distance = std::sqrt(stack.toCamera.x * stack.toCamera.x + stack.toCamera.y * stack.toCamera.y +
                               stack.toCamera.z * stack.toCamera.z);
    if (distance < 1.0f) return false;
    if (g_overlay) {
        // Ce point n'est pas forcément à l'image : derrière la caméra, le moteur n'en ferait rien de bon.
        float ahead = -(stack.toCamera.x * view.x + stack.toCamera.y * view.y + stack.toCamera.z * view.z);
        if (!std::isfinite(ahead) || ahead < distance * kOverlayCone) return false;
        if (distance > kOverlayDistance) {
            // Même direction vue de la caméra, plus près, plus petit : même place et même taille à
            // l'écran, mais devant le mur qui cache le joueur.
            float k = kOverlayDistance / distance;
            stack.toCamera = {stack.toCamera.x * k, stack.toCamera.y * k, stack.toCamera.z * k};
            stack.base = {eye.x - stack.toCamera.x, eye.y - stack.toCamera.y, eye.z - stack.toCamera.z};
            stack.scale = k;
            distance = kOverlayDistance;
        }
    }
    stack.step = distance / kLayerStepRange;
    if (stack.step < kMinLayerStep) stack.step = kMinLayerStep;
    if (stack.step > kMaxLayerStep) stack.step = kMaxLayerStep;
    return true;
}

// États de rendu de nos rectangles : tracé immédiat, inscrit dans la profondeur. Rend le mode de
// sprite du moteur, à redonner à EndSprites.
static uint32_t BeginSprites() {
    uint32_t spriteMode = fn::GeomStateGet(off::kGeomStateSpriteMode);
    fn::GeomStateSet(off::kGeomStateSpriteMode, 0);
    fn::GeomStateCommit();
    fn::DrawStatePush();
    fn::DrawStateSetInt(off::kDrawStateSubmitMode, 0);
    fn::DrawStateSetInt(off::kDrawStateZWrite, 1);
    return spriteMode;
}

static void EndSprites(uint32_t spriteMode) {
    fn::DrawStatePop();
    fn::GeomStateSet(off::kGeomStateSpriteMode, spriteMode);
    fn::GeomStateCommit();
}

// Une étiquette : un cartouche sombre, inscrit dans la profondeur pour que le décor tracé ensuite
// derrière lui ne le recouvre pas, puis les lettres, dont les cases se chevauchent et qui ne s'y
// inscrivent donc pas. first : première des deux couches qu'elle occupe dans la pile.
static void LabelLayers(const Stack& stack, int first, const char* text, float scale, uint32_t color,
                        uint32_t plate = kPlateColor, uint32_t plateAlpha = kPlateAlpha) {
    int length = static_cast<int>(strlen(text));
    if (length > kMaxLabel) length = kMaxLabel;
    if (!g_lettersReady || length <= 0) return;
    float half = kLetterHalf * scale;
    float plateWidth = (static_cast<float>(length) * kLetterAdvance * 0.5f + 0.35f) * half;
    fn::DrawStateSetInt(off::kDrawStateZWrite, 1);
    Layer(stack, first, g_solid, plateWidth, half * 1.15f, plate, plateAlpha);
    fn::DrawStateSetInt(off::kDrawStateZWrite, 0);
    for (int i = 0; i < length; i++) {
        int letter = LetterOf(text[i]);
        if (letter < 0) continue;
        float offset = (static_cast<float>(i) - static_cast<float>(length - 1) * 0.5f) * kLetterAdvance;
        Layer(stack, first + 1, g_letters[letter], half, half, color, 255, offset);
    }
}

// La barre, remplie à filled (de 0 à 1) dans la couleur donnée, et par-dessus le texte : chiffres
// et barre oblique seulement. label : étiquette au-dessus de la barre, ou nul.
static void DrawBar(Character* chr, float filled, uint32_t color, float scale, const char* text,
                    const char* label = nullptr) {
    if (!Ready()) return;
    if (filled < 0.0f) filled = 0.0f;
    if (filled > 1.0f) filled = 1.0f;

    Vector3 anchor;
    fn::OverheadAnchor(chr, &anchor);
    anchor.y += kHeight;
    Stack stack;
    if (!MakeStack(anchor, stack)) return;  // la caméra est dans la jauge
    uint32_t spriteMode = BeginSprites();

    float halfWidth = kHalfWidth * scale, halfHeight = kHalfHeight * scale, border = kBorder * scale;
    Layer(stack, 0, g_solid, halfWidth + border, halfHeight + border, kBorderColor, 170);
    Layer(stack, 1, g_solid, halfWidth, halfHeight, kEmptyColor, 230);
    if (filled > 0.0f) Layer(stack, 2, g_solid, halfWidth, halfHeight, color, 255, 0.0f, 1.0f - filled);

    // Les cases des chiffres se chevauchent : inscrite dans la profondeur, la marge transparente de
    // l'une rognerait sa voisine. La barre, dessous, y est déjà.
    fn::DrawStateSetInt(off::kDrawStateZWrite, 0);
    int length = static_cast<int>(strlen(text));
    for (int i = 0; i < length; i++) {
        int glyph = text[i] == '/' ? kSlash : text[i] - '0';
        if (glyph < 0 || glyph >= kGlyphCount) continue;
        float offset = (static_cast<float>(i) - static_cast<float>(length - 1) * 0.5f) * kDigitAdvance;
        Layer(stack, 3, g_glyphs[glyph], kDigitHalfWidth * scale, kDigitHalfHeight * scale, 0xFFFFFFFF, 255, offset);
    }
    if (label && *label) {
        // Juste au-dessus du cadre de la barre.
        Vector3 above = anchor;
        above.y += (kHalfHeight + kBorder + kLetterHalf * 1.3f) * scale;
        Stack upper;
        if (MakeStack(above, upper)) LabelLayers(upper, 0, label, scale, kLabelColor);
    }
    EndSprites(spriteMode);
}

void Draw(Character* chr, int32_t health, int32_t maxHealth, float scale, bool digits, uint32_t color,
          const char* label) {
    if (maxHealth <= 0) return;
    if (health < 0) health = 0;
    char text[24] = "";
    if (digits) snprintf(text, sizeof(text), "%d/%d", static_cast<int>(health), static_cast<int>(maxHealth));
    DrawBar(chr, static_cast<float>(health) / static_cast<float>(maxHealth), color, scale, text, label);
}

float ReservesHeight() { return kRowGap + 2.0f * (kSlotHalf + kRowPad); }

// Une seule rangée de six cases carrées, centrée juste au-dessus de la jauge : un fond sombre
// inscrit dans la profondeur, pour que le décor tracé ensuite derrière lui ne la recouvre pas, une
// case sombre par emplacement, et les carrés de couleur. Rouge depuis la gauche pour les chargeurs
// de balles, vert depuis la droite pour les cellules d'énergie : tant qu'il y en a six en tout au
// plus, chacun a sa case. Au-delà (le moteur en laisse porter six de chaque) les cases du milieu
// se partagent, moitié gauche rouge, moitié droite verte.
void DrawReserves(Character* chr, int clips, int cells, float scale) {
    if (!Ready()) return;
    if (clips < 0) clips = 0;
    if (cells < 0) cells = 0;
    if (clips > kReserveSlots) clips = kReserveSlots;
    if (cells > kReserveSlots) cells = kReserveSlots;

    Vector3 center;
    fn::OverheadAnchor(chr, &center);
    center.y += kHeight + (kHalfHeight + kBorder + kRowGap + kSlotHalf + kRowPad) * scale;
    Stack stack;
    if (!MakeStack(center, stack)) return;
    uint32_t spriteMode = BeginSprites();

    float half = kSlotHalf * scale, fill = half * kSlotInset;
    float rowHalf = (kSlotSpacing * (kReserveSlots - 1) * 0.5f + kSlotHalf + kRowPad) * scale;
    fn::DrawStateSetInt(off::kDrawStateZWrite, 1);
    Layer(stack, 0, g_solid, rowHalf, half + kRowPad * scale, kBorderColor, kRowAlpha);
    fn::DrawStateSetInt(off::kDrawStateZWrite, 0);
    for (int i = 0; i < kReserveSlots; i++) {
        // Centre de la case, en unités du monde depuis le milieu de la rangée ; le tracé le veut en
        // demi-largeurs du rectangle tracé.
        float x = (static_cast<float>(i) - static_cast<float>(kReserveSlots - 1) * 0.5f) * kSlotSpacing * scale;
        Layer(stack, 1, g_solid, half, half, kSlotColor, 220, x / half);
        bool red = i < clips, green = i >= kReserveSlots - cells;
        if (red && green) {
            float narrow = fill * 0.5f;
            Layer(stack, 2, g_solid, narrow, fill, kClipColor, 255, (x - narrow) / narrow);
            Layer(stack, 2, g_solid, narrow, fill, kCellColor, 255, (x + narrow) / narrow);
        } else if (red || green) {
            Layer(stack, 2, g_solid, fill, fill, red ? kClipColor : kCellColor, 255, x / fill);
        }
    }
    EndSprites(spriteMode);
}

void DrawLabelAt(const Vector3& position, const char* text, float scale, uint32_t color) {
    if (!text || !*text || !Ready() || !g_lettersReady) return;
    Stack stack;
    if (!MakeStack(position, stack)) return;
    uint32_t spriteMode = BeginSprites();
    LabelLayers(stack, 0, text, scale, color);
    EndSprites(spriteMode);
}

void DrawLabel(Character* chr, const char* text, float scale, uint32_t color) {
    Vector3 anchor;
    fn::OverheadAnchor(chr, &anchor);
    anchor.y += kHeight;
    DrawLabelAt(anchor, text, scale, color);
}

void DrawNameplate(Character* chr, const char* text, float scale, uint32_t color, float lift) {
    Vector3 anchor;
    fn::OverheadAnchor(chr, &anchor);
    // Juste au-dessus du cadre de la barre, comme l'étiquette d'un PNJ, ou de ce qui la surmonte.
    anchor.y += kHeight + (kHalfHeight + kBorder + kLetterHalf * 1.3f + lift) * scale;
    DrawLabelAt(anchor, text, scale, color);
}

// -------------------------------------------------------------------------------------------------
// Joueur à terre
// -------------------------------------------------------------------------------------------------
static float Clamp01(float value) { return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); }

// Couleur entre a (t = 0) et b (t = 1), composante par composante.
static uint32_t Blend(uint32_t a, uint32_t b, float t) {
    t = Clamp01(t);
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        float from = static_cast<float>((a >> shift) & 0xFF), to = static_cast<float>((b >> shift) & 0xFF);
        out |= static_cast<uint32_t>(from + (to - from) * t + 0.5f) << shift;
    }
    return out;
}

// Au-delà de kBeaconRange, le panneau grandit avec la distance : il garde à l'écran la taille qu'il
// a à cette portée, et se repère de l'autre bout du niveau.
static float BeaconScale(const Vector3& anchor) {
    void* camera = *reinterpret_cast<void**>(addr::kSceneCameraPtr);
    if (!camera) return 1.0f;
    Vector3 eye, view = {};
    fn::CameraView(camera, &eye, &view, nullptr);
    float dx = eye.x - anchor.x, dy = eye.y - anchor.y, dz = eye.z - anchor.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distance) || distance <= kBeaconRange) return 1.0f;
    float scale = distance / kBeaconRange;
    return scale > kBeaconMax ? kBeaconMax : scale;
}

void DrawDowned(Character* chr, const Downed& downed, uint32_t playerColor) {
    if (!Ready()) return;
    Vector3 anchor;
    fn::OverheadAnchor(chr, &anchor);
    anchor.y += kHeight;
    // Tout le panneau à la même échelle : ses lignes s'écartent autant qu'elles grandissent.
    float scale = kDownedScale * (downed.beacon ? BeaconScale(anchor) : 1.0f);
    float pulse = Clamp01(downed.pulse), remaining = Clamp01(downed.remaining);
    bool filling = downed.progress >= 0.0f;  // on le ranime : la barre montre où en est la réanimation
    uint32_t spriteMode = BeginSprites();

    // La barre : filet, fond, ce qui reste (ou ce qui est fait), graduations, secondes.
    float halfWidth = kHalfWidth * scale, halfHeight = kHalfHeight * scale, border = kBorder * scale;
    Stack stack;
    if (MakeStack(anchor, stack)) {
        Layer(stack, 0, g_solid, halfWidth + border, halfHeight + border, kFrameColor, 255);
        Layer(stack, 1, g_solid, halfWidth, halfHeight, kPanelColor, 240);
        float filled = filling ? Clamp01(downed.progress) : remaining;
        uint32_t color = kProgressColor;
        if (!filling) color = downed.reviving ? kReviveColor : Blend(kBleedLowColor, kBleedColor, (remaining - 0.15f) / 0.35f);
        // Sur la fin, la barre bat avec le repère.
        bool urgent = !filling && !downed.reviving && remaining < kBleedLow;
        uint32_t alpha = urgent ? static_cast<uint32_t>(150.0f + 105.0f * pulse) : 255;
        if (filled > 0.0f) Layer(stack, 2, g_solid, halfWidth, halfHeight, color, alpha, 0.0f, 1.0f - filled);
        float tick = kTickHalf * scale;
        for (int i = 1; i < kBarTicks; i++) {
            float x = (2.0f * static_cast<float>(i) / static_cast<float>(kBarTicks) - 1.0f) * halfWidth;
            Layer(stack, 3, g_solid, tick, halfHeight, kPanelColor, 255, x / tick);
        }
        // Les cases des chiffres se chevauchent : elles ne s'inscrivent pas dans la profondeur.
        fn::DrawStateSetInt(off::kDrawStateZWrite, 0);
        char text[8];
        int shown = static_cast<int>(std::ceil(downed.seconds < 0.0f ? 0.0f : downed.seconds));
        snprintf(text, sizeof(text), "%d", shown > 999 ? 999 : shown);
        int length = static_cast<int>(strlen(text));
        for (int i = 0; i < length; i++) {
            float offset = (static_cast<float>(i) - static_cast<float>(length - 1) * 0.5f) * kDigitAdvance;
            Layer(stack, 4, g_glyphs[text[i] - '0'], kDigitHalfWidth * scale, kDigitHalfHeight * scale, 0xFFFFFFFF, 255,
                  offset);
        }
    }

    // Au-dessus : le titre, rouge qui bat, ou bleu clair dès qu'on le ranime ; puis son pseudo.
    float line = kLetterHalf * 1.3f * scale;  // du bord de la barre au milieu d'une ligne de texte
    uint32_t accent = filling || downed.reviving ? kProgressColor : Blend(kBleedLowColor, 0xFFFFFFFF, pulse * 0.7f);
    Vector3 at = anchor;
    at.y += (kHalfHeight + kBorder) * scale + line;
    if (downed.title && *downed.title) {
        if (MakeStack(at, stack)) LabelLayers(stack, 0, downed.title, scale, accent, kPanelColor, kPanelAlpha);
        at.y += 2.0f * line;
    }
    if (downed.name && *downed.name) {
        if (MakeStack(at, stack)) LabelLayers(stack, 0, downed.name, scale, playerColor, kPanelColor, kPanelAlpha);
        at.y += 2.0f * line;
    }
    // Le repère, au-dessus de tout, sur un fond inscrit dans la profondeur ; il bat.
    if (g_icon) {
        float plate = kIconHalf * 1.2f * scale;
        at.y += plate - line * 0.75f;
        if (MakeStack(at, stack)) {
            float half = kIconHalf * scale * (0.85f + 0.3f * pulse);
            fn::DrawStateSetInt(off::kDrawStateZWrite, 1);
            Layer(stack, 0, g_solid, plate, plate, kPanelColor, kPanelAlpha);
            fn::DrawStateSetInt(off::kDrawStateZWrite, 0);
            Layer(stack, 1, g_icon, half, half, accent, 255);
        }
    }
    // Dessous : la touche à tenir.
    if (downed.hint && *downed.hint) {
        Vector3 below = anchor;
        below.y -= (kHalfHeight + kBorder) * scale + line * kHintScale;
        if (MakeStack(below, stack)) {
            LabelLayers(stack, 0, downed.hint, scale * kHintScale, 0xFFFFFFFF, kPanelColor, kPanelAlpha);
        }
    }
    EndSprites(spriteMode);
}

}  // namespace gauge
