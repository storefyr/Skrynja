# Release Guide for Skrynja

Guide pour créer une nouvelle release de Skrynja.

## Prérequis

- Accès en écriture au repo GitHub
- Branche main/master à jour
- Tous les changements testés localement

## Process de Release

### 1. Préparer le CHANGELOG

Mettre à jour `CHANGELOG.md` avec les changements de la version :

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- Nouvelle fonctionnalité 1
- Nouvelle fonctionnalité 2

### Changed
- Modification 1
- Modification 2

### Fixed
- Bug fix 1
- Bug fix 2
```

### 2. Mettre à jour la version

Mettre à jour la version dans les fichiers suivants :

**harbour-skrynja.yaml** (ligne 4):
```yaml
Version: X.Y.Z
```

**rpm/harbour-skrynja.spec** (ligne 4):
```spec
Version:    X.Y.Z
```

**qml/pages/AboutPage.qml** (environ ligne 37):
```qml
Label {
    text: "Version X.Y.Z"
    ...
}
```

### 3. Commit les changements

```bash
git add CHANGELOG.md harbour-skrynja.yaml rpm/harbour-skrynja.spec qml/pages/AboutPage.qml
git commit -m "chore: Bump version to X.Y.Z"
git push origin main
```

### 4. Créer et pousser le tag

```bash
# Créer le tag avec versioning sémantique
git tag -a vX.Y.Z -m "Release vX.Y.Z"

# Pousser le tag vers GitHub
git push origin vX.Y.Z
```

**⚠️ Format du tag:** Le tag DOIT être au format `vX.Y.Z` (avec le `v` en préfixe) pour déclencher le workflow.

### 5. GitHub Actions prend le relais

Une fois le tag poussé, le workflow **build-docker.yml** se déclenche automatiquement :

1. **Build aarch64**
   - aarch64 (Jolla Phone 2026, Jolla C2, Xperia 10 II/III/IV)

2. **Génération du changelog**
   - Extrait automatiquement les commits depuis le dernier tag
   - Formate en markdown

3. **Création de la Release GitHub**
   - Attach le RPM package aarch64
   - Génère des notes de release professionnelles
   - Publie automatiquement (non-draft)

### 6. Vérifier la Release

1. Aller sur [GitHub Releases](https://github.com/storefyr/Skrynja/releases)
2. Vérifier que la release apparaît avec `vX.Y.Z`
3. Vérifier que le RPM aarch64 est attaché
4. Tester le téléchargement du RPM, ou utiliser `sh update.sh X.Y.Z` sur le device

### 7. Annoncer la Release

Optionnel mais recommandé :

- **Sailfish Forum**: Poster sur https://forum.sailfishos.org/
- **Jolla Together**: Partager sur https://together.jolla.com/
- **Reddit**: r/SailfishOS
- **Twitter/Mastodon**: Utiliser #SailfishOS

Template d'annonce :
```
🎉 Skrynja vX.Y.Z est disponible !

🐱⛵ Skrynja est un client natif Sailfish OS pour Le Chat de Mistral AI.

✨ Nouveautés :
- [Liste des features]

📥 Télécharger : https://github.com/storefyr/Skrynja/releases/tag/vX.Y.Z

#SailfishOS #MistralAI #OpenSource
```

## Versioning Sémantique

Skrynja suit le [Semantic Versioning 2.0.0](https://semver.org/):

- **MAJOR** (X.0.0) : Changements incompatibles avec l'API
- **MINOR** (0.Y.0) : Nouvelles fonctionnalités rétro-compatibles
- **PATCH** (0.0.Z) : Corrections de bugs rétro-compatibles

### Exemples

- `v1.0.0` → `v1.0.1` : Bug fixes seulement
- `v1.0.1` → `v1.1.0` : Ajout de support d'images, conversations persistantes
- `v1.1.0` → `v2.0.0` : Changement de structure de données incompatible

## Rollback d'une Release

Si une release a un problème critique :

### Option 1 : Hotfix rapide (Recommandé)

```bash
# Créer une branche hotfix
git checkout -b hotfix/vX.Y.Z+1 vX.Y.Z

# Corriger le bug
# ... modifications ...

git commit -m "fix: Critical bug in feature X"

# Merger dans main
git checkout main
git merge hotfix/vX.Y.Z+1

# Créer un nouveau tag patch
git tag -a vX.Y.Z+1 -m "Hotfix vX.Y.Z+1"
git push origin vX.Y.Z+1
```

### Option 2 : Marquer comme pre-release

1. Aller sur GitHub Releases
2. Éditer la release problématique
3. Cocher "Set as a pre-release"
4. Sauvegarder

### Option 3 : Supprimer la release

```bash
# Supprimer le tag local
git tag -d vX.Y.Z

# Supprimer le tag distant
git push origin :refs/tags/vX.Y.Z

# Supprimer la release sur GitHub (manuellement dans l'UI)
```

⚠️ Éviter de supprimer des releases si des utilisateurs ont déjà téléchargé.

## Checklist de Release

Utiliser cette checklist avant chaque release :

```markdown
## Pre-Release
- [ ] Tous les tests passent localement
- [ ] CHANGELOG.md mis à jour
- [ ] Version bumped dans yaml, spec, et AboutPage.qml
- [ ] README à jour si nécessaire
- [ ] Pas de TODO ou FIXME dans le code
- [ ] Commit "Bump version" créé et poussé

## Release
- [ ] Tag créé au format vX.Y.Z
- [ ] Tag poussé vers GitHub
- [ ] GitHub Actions workflow déclenché
- [ ] Tous les builds (3 archs) réussis

## Post-Release
- [ ] Release visible sur GitHub
- [ ] 3 RPM packages attachés et téléchargeables
- [ ] Notes de release correctes
- [ ] RPM testé sur un device
- [ ] Annonce sur forum/reddit/social media
```

## Troubleshooting

### Le workflow ne se déclenche pas

**Cause:** Tag ne correspond pas au pattern `v*.*.*`

**Solution:**
```bash
git tag -d vX.Y.Z  # Supprimer le tag local
git tag -a v1.0.0 -m "Release v1.0.0"  # Recréer avec bon format
git push origin v1.0.0
```

### Build échoue sur une architecture

**Cause:** Dépendance manquante ou erreur de compilation

**Solution:**
1. Vérifier les logs du workflow sur GitHub Actions
2. Tester localement avec `sfdk build` pour cette arch
3. Corriger le problème
4. Créer un hotfix et re-release

### RPM généré mais ne s'installe pas

**Cause:** Dépendances runtime manquantes dans le .spec

**Solution:**
1. Ajouter la dépendance dans `rpm/harbour-skrynja.spec`
2. Bump la version patch
3. Re-release

## Automatisation Future

Idées pour améliorer le process :

- [ ] Script `scripts/release.sh` pour automatiser steps 1-4
- [ ] Pre-commit hook pour vérifier cohérence des versions
- [ ] Bot Discord/Telegram pour notifications de release
- [ ] Validation des RPM avant publication (smoke tests)
- [ ] Mirror sur OpenRepos.net

## Resources

- [GitHub Releases](https://github.com/storefyr/Skrynja/releases)
- [GitHub Actions Workflows](https://github.com/storefyr/Skrynja/actions)
- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)

---

Questions? Ouvrir une issue sur GitHub.
