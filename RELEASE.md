# Udgivelsesguide til Skrynja

Guide til at oprette en ny udgivelse af Skrynja.

## Forudsætninger

- Skriveadgang til GitHub-repoet
- main-branchen er opdateret
- Alle ændringer testet lokalt

## Udgivelsesproces

### 1. Forbered CHANGELOG

Opdater `CHANGELOG.md` med ændringerne i versionen:

```markdown
## [X.Y.Z] - ÅÅÅÅ-MM-DD

### Added
- Ny funktion 1
- Ny funktion 2

### Changed
- Ændring 1
- Ændring 2

### Fixed
- Fejlrettelse 1
- Fejlrettelse 2
```

### 2. Opdater versionen

Opdater versionen i følgende filer:

**harbour-skrynja.yaml** (linje 4):
```yaml
Version: X.Y.Z
```

**rpm/harbour-skrynja.spec** (linje 4):
```spec
Version:    X.Y.Z
```

**harbour-skrynja.pro** (fallback-version, omkring linje 10):
```qmake
VERSION = X.Y.Z
```

Bemærk: Versionen i appen vises i Indstillinger via `settingsManager.appVersion`, som læser `APP_VERSION` defineret i `.pro`-filen — der findes ingen separat `AboutPage.qml`.

### 3. Commit ændringerne

```bash
git add CHANGELOG.md harbour-skrynja.yaml harbour-skrynja.pro rpm/harbour-skrynja.spec
git commit -m "chore: Bump version to X.Y.Z"
git push origin main
```

### 4. Opret og push tagget

```bash
# Opret tagget med semantisk versionering
git tag -a vX.Y.Z -m "Release vX.Y.Z"

# Push tagget til GitHub
git push origin vX.Y.Z
```

**⚠️ Tag-format:** Tagget SKAL have formatet `vX.Y.Z` (med `v` foran) for at udløse workflowet.

### 5. GitHub Actions overtager

Når tagget er pushet, udløses workflowet **build-docker.yml** automatisk:

1. **Build aarch64**
   - aarch64 (Jolla Phone 2026, Jolla C2, Xperia 10 II/III/IV)

2. **Generering af changelog**
   - Udtrækker automatisk commits siden seneste tag
   - Formaterer som markdown

3. **Oprettelse af GitHub-release**
   - Vedhæfter aarch64-RPM-pakken
   - Genererer udgivelsesnoter
   - Udgiver automatisk (ikke-draft)

### 6. Verificer udgivelsen

1. Gå til [GitHub Releases](https://github.com/storefyr/Skrynja/releases)
2. Verificer at udgivelsen vises med `vX.Y.Z`
3. Verificer at aarch64-RPM'en er vedhæftet
4. Test download af RPM'en, eller brug `sh update.sh X.Y.Z` på enheden

### 7. Annoncer udgivelsen

Valgfrit, men anbefalet:

- **Sailfish Forum**: Post på https://forum.sailfishos.org/
- **Jolla Together**: Del på https://together.jolla.com/
- **Reddit**: r/SailfishOS
- **Mastodon**: Brug #SailfishOS

Annonceskabelon:
```
🎉 Skrynja vX.Y.Z er udgivet!

🐱⛵ Skrynja er en native Sailfish OS-klient til Mistral AI's Le Chat.

✨ Nyt:
- [Liste over funktioner]

📥 Download: https://github.com/storefyr/Skrynja/releases/tag/vX.Y.Z

#SailfishOS #MistralAI #OpenSource
```

## Semantisk versionering

Skrynja følger [Semantic Versioning 2.0.0](https://semver.org/):

- **MAJOR** (X.0.0): Ændringer, der ikke er bagudkompatible
- **MINOR** (0.Y.0): Nye, bagudkompatible funktioner
- **PATCH** (0.0.Z): Bagudkompatible fejlrettelser

### Eksempler

- `v1.0.0` → `v1.0.1`: Kun fejlrettelser
- `v1.0.1` → `v1.1.0`: Tilføjelse af billedunderstøttelse, persistente samtaler
- `v1.1.0` → `v2.0.0`: Ændring af datastruktur, der ikke er bagudkompatibel

## Rollback af en udgivelse

Hvis en udgivelse har et kritisk problem:

### Mulighed 1: Hurtig hotfix (anbefalet)

```bash
# Opret en hotfix-branch
git checkout -b hotfix/vX.Y.Z+1 vX.Y.Z

# Ret fejlen
# ... ændringer ...

git commit -m "fix: Critical bug in feature X"

# Merge ind i main
git checkout main
git merge hotfix/vX.Y.Z+1

# Opret et nyt patch-tag
git tag -a vX.Y.Z+1 -m "Hotfix vX.Y.Z+1"
git push origin vX.Y.Z+1
```

### Mulighed 2: Marker som pre-release

1. Gå til GitHub Releases
2. Rediger den problematiske udgivelse
3. Sæt flueben ved "Set as a pre-release"
4. Gem

### Mulighed 3: Slet udgivelsen

```bash
# Slet tagget lokalt
git tag -d vX.Y.Z

# Slet tagget på GitHub
git push origin :refs/tags/vX.Y.Z

# Slet udgivelsen på GitHub (manuelt i UI'et)
```

⚠️ Undgå at slette udgivelser, hvis brugere allerede har downloadet dem.

## Tjekliste før udgivelse

Brug denne tjekliste før hver udgivelse:

```markdown
## Før udgivelse
- [ ] Alle tests består lokalt
- [ ] CHANGELOG.md opdateret
- [ ] Version opdateret i yaml, pro, spec
- [ ] README opdateret, hvis nødvendigt
- [ ] Ingen TODO eller FIXME i koden
- [ ] Commit "Bump version" oprettet og pushet

## Udgivelse
- [ ] Tag oprettet i formatet vX.Y.Z
- [ ] Tag pushet til GitHub
- [ ] GitHub Actions workflow udløst
- [ ] Build (aarch64) gennemført

## Efter udgivelse
- [ ] Udgivelse synlig på GitHub
- [ ] aarch64-RPM vedhæftet og kan downloades
- [ ] Udgivelsesnoter korrekte
- [ ] RPM testet på en enhed
- [ ] Annonceret på forum/reddit/sociale medier
```

## Fejlfinding

### Workflowet udløses ikke

**Årsag:** Tagget matcher ikke mønsteret `v*.*.*`

**Løsning:**
```bash
git tag -d vX.Y.Z  # Slet det lokale tag
git tag -a v1.0.0 -m "Release v1.0.0"  # Opret igen med korrekt format
git push origin v1.0.0
```

### Build fejler på en arkitektur

**Årsag:** Manglende afhængighed eller kompileringsfejl

**Løsning:**
1. Tjek workflow-loggene på GitHub Actions
2. Test lokalt med `sfdk build` for den arkitektur
3. Ret problemet
4. Opret en hotfix og udgiv igen

### RPM genereres, men installeres ikke

**Årsag:** Manglende runtime-afhængigheder i .spec-filen

**Løsning:**
1. Tilføj afhængigheden i `rpm/harbour-skrynja.spec`
2. Bump patch-versionen
3. Udgiv igen

## Fremtidig automatisering

Idéer til at forbedre processen:

- [ ] Script `scripts/release.sh` til at automatisere trin 1-4
- [ ] Pre-commit hook til at verificere versionskonsistens
- [ ] Mirror på OpenRepos.net

## Ressourcer

- [GitHub Releases](https://github.com/storefyr/Skrynja/releases)
- [GitHub Actions Workflows](https://github.com/storefyr/Skrynja/actions)
- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)

---

Spørgsmål? Opret en issue på GitHub.
