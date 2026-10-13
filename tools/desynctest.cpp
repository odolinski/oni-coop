// Test du détecteur de désynchronisation sans le jeu : des suites de rapports du client sont
// données au comparateur de l'hôte, et l'on vérifie ce qu'il décide de renvoyer, et quand.
#include <cstdio>

#include "../src/desynccheck.h"

static int g_failures = 0;

static void Expect(bool condition, const char* what) {
    if (condition) return;
    g_failures++;
    printf("ECHEC : %s\n", what);
}

static proto::Digest Same() {
    proto::Digest digest = {};
    digest.levelHash = 0x626092F4;
    for (int part = 0; part < proto::kDigestParts; part++) digest.parts[part] = 100u + static_cast<uint32_t>(part);
    return digest;
}

static const uint32_t kNpcs = 1u << proto::kDigestNpcs;
static const uint32_t kGround = 1u << proto::kDigestGround;
static const uint32_t kScripts = 1u << proto::kDigestScripts;

int main() {
    desync::Checker checker;

    // 1. Deux machines identiques : jamais rien à renvoyer.
    uint32_t any = 0;
    for (int second = 0; second < 30; second++) {
        desync::Verdict verdict = checker.Compare(Same(), Same(), second);
        any |= verdict.repair | verdict.persistent;
    }
    Expect(any == 0, "etats identiques : aucun renvoi");

    // 2. Un PNJ manque chez le client et n'arrive jamais : renvoi au 4e rapport divergent, trois
    //    fois en tout à cinq secondes au moins d'intervalle, puis un seul signalement.
    checker.Reset();
    proto::Digest theirs = Same();
    theirs.parts[proto::kDigestNpcs] = 7;
    int repairs = 0, persistents = 0, firstRepair = -1, lastRepair = -100;
    bool spaced = true, onlyNpcs = true;
    for (int second = 0; second < 60; second++) {
        desync::Verdict verdict = checker.Compare(Same(), theirs, second);
        onlyNpcs = onlyNpcs && (verdict.repair & ~kNpcs) == 0 && (verdict.persistent & ~kNpcs) == 0;
        if (verdict.repair) {
            if (firstRepair < 0) firstRepair = second;
            spaced = spaced && second - lastRepair >= 5;
            lastRepair = second;
            repairs++;
        }
        if (verdict.persistent) persistents++;
    }
    // Le premier rapport n'a pas de précédent : la série commence au deuxième.
    Expect(firstRepair == desync::kStreak, "ecart durable : premier renvoi au 4e rapport comparable");
    Expect(repairs == desync::kMaxTries, "ecart durable : trois renvois, pas plus");
    Expect(spaced, "ecart durable : cinq secondes au moins entre deux renvois");
    Expect(persistents == 1, "ecart durable : signale une seule fois");
    Expect(onlyNpcs, "ecart durable : seule la partie en cause est renvoyee");

    // 3. Le sol change chez l'hôte à chaque rapport (bonus qui tombent) et le client a toujours
    //    un temps de retard : pas un écart, juste le délai.
    checker.Reset();
    any = 0;
    for (int second = 0; second < 30; second++) {
        proto::Digest ours = Same();
        proto::Digest behind = Same();
        ours.parts[proto::kDigestGround] = 1000u + static_cast<uint32_t>(second);
        behind.parts[proto::kDigestGround] = 999u + static_cast<uint32_t>(second);
        desync::Verdict verdict = checker.Compare(ours, behind, second);
        any |= verdict.repair | verdict.persistent;
    }
    Expect(any == 0, "partie qui bouge chez l'hote : aucun renvoi");

    // 4. L'autre joueur est dans un autre niveau : rien n'est comparable.
    checker.Reset();
    any = 0;
    for (int second = 0; second < 30; second++) {
        proto::Digest elsewhere = Same();
        elsewhere.levelHash = 0xD3CAB9F7;
        elsewhere.parts[proto::kDigestNpcs] = 1;
        elsewhere.parts[proto::kDigestGround] = 2;
        desync::Verdict verdict = checker.Compare(Same(), elsewhere, second);
        any |= verdict.repair | verdict.persistent;
    }
    Expect(any == 0, "niveaux differents : aucun renvoi");

    // 5. Un écart corrigé par le renvoi : le compteur repart, un nouvel écart est de nouveau
    //    renvoyé trois fois.
    checker.Reset();
    theirs = Same();
    theirs.parts[proto::kDigestGround] = 5;
    repairs = 0;
    for (int second = 0; second < 6; second++) repairs += checker.Compare(Same(), theirs, second).repair ? 1 : 0;
    Expect(repairs == 1, "ecart : un renvoi dans les six premieres secondes");
    Expect(checker.Compare(Same(), Same(), 6).repair == 0 && checker.tries(proto::kDigestGround) == 0,
           "ecart corrige : compteur remis a zero");
    repairs = 0;
    for (int second = 7; second < 60; second++) {
        desync::Verdict verdict = checker.Compare(Same(), theirs, second);
        repairs += (verdict.repair & kGround) ? 1 : 0;
    }
    Expect(repairs == desync::kMaxTries, "nouvel ecart : de nouveau trois renvois");

    // 6. Scripts : rien à renvoyer, un signalement et c'est tout.
    checker.Reset();
    theirs = Same();
    theirs.parts[proto::kDigestScripts] = 1;
    repairs = 0;
    persistents = 0;
    for (int second = 0; second < 60; second++) {
        desync::Verdict verdict = checker.Compare(Same(), theirs, second);
        repairs += verdict.repair ? 1 : 0;
        persistents += (verdict.persistent & kScripts) ? 1 : 0;
    }
    Expect(repairs == 0 && persistents == 1, "scripts : signale une fois, jamais renvoye");

    // 7. L'hôte sort de son menu de pause et lit d'un coup dix rapports du client, tous en retard
    //    sur un réglage qu'il vient de changer : une rafale ne vaut pas quatre observations. Vu en
    //    jeu en 0.13, où elle déclenchait un renvoi pour rien.
    checker.Reset();
    for (int second = 0; second < 5; second++) checker.Compare(Same(), Same(), second);
    proto::Digest changed = Same();
    changed.parts[proto::kDigestSettings] = 0xF;
    any = 0;
    for (int report = 0; report < 10; report++) {
        desync::Verdict verdict = checker.Compare(changed, Same(), 12.0);
        any |= verdict.repair | verdict.persistent;
    }
    Expect(any == 0, "rapports en rafale : aucun renvoi");
    // Le client a suivi : rien non plus ensuite.
    for (int second = 13; second < 20; second++) any |= checker.Compare(changed, changed, second).repair;
    Expect(any == 0, "rapports en rafale : rien apres que le client a suivi");
    // Un vrai écart, lui, reste vu : les rapports suivants arrivent un par seconde.
    repairs = 0;
    for (int second = 20; second < 30; second++) repairs += checker.Compare(changed, Same(), second).repair ? 1 : 0;
    Expect(repairs >= 1, "apres une rafale : un ecart durable est encore renvoye");

    printf("%s\n", g_failures == 0 ? "OK" : "ECHEC");
    return g_failures == 0 ? 0 : 1;
}
