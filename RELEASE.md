# Udgivelsesguide til Skrynja

Guide til at oprette en ny udgivelse af Skrynja.

## Forudsætninger

- Skriveadgang til GitHub-repoet
- `main` er opdateret, og CI er grøn
- Alle ændringer er testet

## Udgivelsesproces

### 1. Opdater versionen

Opdater versionen tre steder:

**harbour-skrynja.yaml** (linje 4):
```yaml
Version: X.Y.Z
```

**rpm/harbour-skrynja.spec** (linje 4):
```spec
Version: X.Y.Z
```

**harbour-skrynja.pro** (omkring linje 10):
```qmake
VERSION = X.Y.Z
```

Versionen i appen vises i Indstillinger via `settingsManager.appVersion`, som
læser `APP_VERSION` defineret i `.pro`-filen. Der findes ingen `AboutPage.qml`.

### 2. Commit ændringerne

```bash
git add harbour-skrynja.yaml harbour-skrynja.pro rpm/harbour-skrynja.spec
git commit -m "chore: Bump version to X.Y.Z"
git push origin main
```

### 3. Opret og push tagget

```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin vX.Y.Z
```

Tagget SKAL have formatet `vX.Y.Z` med `v` foran, ellers udløses workflowet
ikke.

### 4. GitHub Actions overtager

Push af tagget udløser `.github/workflows/build-docker.yml`:

1. **Byg** — aarch64, Sailfish OS-platformsbillede
2. **Enhedsprøver** — QtTest på Ubuntu; fejl blokerer udgivelsen
3. **Udgivelsesnoter** — dannes af commit-beskeder siden seneste tag
4. **GitHub-release** — aarch64-RPM vedhæftes, udgives straks (ikke kladde)

### 5. Verificer udgivelsen

1. Gå til [GitHub Releases](https://github.com/storefyr/Skrynja/releases)
2. Kontrollér at udgivelsen vises med `vX.Y.Z`
3. Kontrollér at aarch64-RPM'en er vedhæftet
4. Test på enheden med `sh update.sh X.Y.Z`

### 6. Annoncer udgivelsen

Valgfrit, men anbefalet:

- **Sailfish Forum**: https://forum.sailfishos.org/
- **Jolla Together**: https://together.jolla.com/
- **Reddit**: r/SailfishOS
- **Mastodon**: #SailfishOS

Annonceskabelon:
```
🎉 Skrynja vX.Y.Z er udgivet!

🐱⛵ Skrynja er en ACP-klient til Sailfish OS.

✨ Nyt:
- [Liste over funktioner]

📥 Download: https://github.com/storefyr/Skrynja/releases/tag/vX.Y.Z

#SailfishOS #OpenSource
```

## Semantisk versionering

Skrynja følger [Semantic Versioning 2.0.0](https://semver.org/):

- **MAJOR** (X.0.0): ændringer, der ikke er bagudkompatible
- **MINOR** (0.Y.0): nye, bagudkompatible funktioner
- **PATCH** (0.0.Z): bagudkompatible fejlrettelser

Eksempler: `v1.0.0` → `v1.0.1` er fejlrettelser; `v1.0.1` → `v1.1.0` er en ny
funktion; `v1.1.0` → `v2.0.0` er en brydende ændring.

## Rollback

### Mulighed 1: Hotfix (anbefalet)

```bash
git checkout -b hotfix/vX.Y.Z+1 vX.Y.Z
# ret fejlen
git commit -m "fix: Critical bug in feature X"
git checkout main
git merge hotfix/vX.Y.Z+1
git tag -a vX.Y.Z+1 -m "Hotfix vX.Y.Z+1"
git push origin vX.Y.Z+1
```

### Mulighed 2: Markér som pre-release

Rediger udgivelsen på GitHub, sæt flueben ved "Set as a pre-release", gem.

### Mulighed 3: Slet udgivelsen

```bash
git tag -d vX.Y.Z
git push origin :refs/tags/vX.Y.Z
```

Slet derefter udgivelsen i GitHub's grænseflade. Undgå det, hvis nogen har
downloadet den.

## Tjekliste før udgivelse

```markdown
## Før udgivelse
- [ ] Alle tests består lokalt og i CI
- [ ] Version opdateret i yaml, pro, spec
- [ ] LÆSMIG opdateret, hvis nødvendigt
- [ ] Ingen TODO eller FIXME i koden
- [ ] Commit "Bump version" oprettet og pushet

## Udgivelse
- [ ] Tag oprettet i formatet vX.Y.Z
- [ ] Tag pushet til GitHub
- [ ] Workflow udløst og grøn
- [ ] Build (aarch64) gennemført

## Efter udgivelse
- [ ] Udgivelse synlig på GitHub
- [ ] RPM vedhæftet og kan downloades
- [ ] Udgivelsesnoter korrekte
- [ ] RPM testet på enheden
- [ ] Annonceret
```

## Fejlfinding

### Workflowet udløses ikke

Tagget matcher ikke mønsteret `v*.*.*`. Slet det, og opret det igen:

```bash
git tag -d vX.Y.Z
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

### Bygget fejler

Læs workflow-loggen. Vil du genskabe bygget lokalt:

```bash
docker run --rm -it -v $(pwd):/home/sailfish/src -w /home/sailfish/src \
  coderus/sailfishos-platform-sdk:5.0.0.43 \
  mb2 -t SailfishOS-5.0.0.43-aarch64 build
```

### RPM genereres, men installeres ikke

Manglende køretidsafhængighed i `rpm/harbour-skrynja.spec`. Tilføj den, hæv
patch-versionen, udgiv igen.

## Ressourcer

- [GitHub Releases](https://github.com/storefyr/Skrynja/releases)
- [GitHub Actions](https://github.com/storefyr/Skrynja/actions)
- [Semantic Versioning](https://semver.org/)

---

Spørgsmål? Opret en issue på GitHub.
