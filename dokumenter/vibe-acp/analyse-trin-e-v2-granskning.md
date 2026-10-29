# Granskning af v2-grundlaget for Trin E (relæ `dd64f635` + tidligere `52b8e70e`)

*(Runde 14. Skrevet mod revision `0870869fcc5f6209aebbefad31fa1a2dfa269456` — se kilde-tabel. Relæer: `52b8e70e` (v2-granskning, otte fund F1–F8, syv lærdomme, åbent A6) og `dd64f635` (forslag om at afløse A6 med ingen-grænse + 16 MiB-logning). Ingen arkitekturvalg genåbnes her; dette dokument bringer målinger og anbefalinger, beslutninger træffes af storefyr.)*

## 0 · Kilder (aliasregel: ét alias pr. revision; løbende tekst = alias + kort SHA + linje)

| Alias | Sti | Fuld commit-SHA | Linjer |
|---|---|---|---|
| S1@0870869 | dokumenter/vibe-acp/spec-trin-e-ndjson-parser.md | 0870869fcc5f6209aebbefad31fa1a2dfa269456 | 291 |
| S2@0870869 | dokumenter/vibe-acp/analyse-a6-begrundet-forkast-ikke-draeb.md | 0870869fcc5f6209aebbefad31fa1a2dfa269456 | 106 |
| S3@0870869 | src/mistralapi.cpp | 0870869fcc5f6209aebbefad31fa1a2dfa269456 | 1 678 |
| H1@97bd5c4 | samme sti som S1 (historisk, retraktionsspor) | 97bd5c438777f4ea5e25d6aa68f28e46e144867b | 269 |
| W1@0870869 | .github/workflows/pr-build.yml | 0870869fcc5f6209aebbefad31fa1a2dfa269456 | 129 |
| R1@0870869 | dokumenter/vibe-acp/README.md | 0870869fcc5f6209aebbefad31fa1a2dfa269456 | 110 |
| SP1@0870869 | rpm/harbour-skrynja.spec | 0870869fcc5f6209aebbefad31fa1a2dfa269456 | 54 |
| PR1@0870869 | harbour-skrynja.pro | 0870869fcc5f6209aebbefad31fa1a2dfa269456 | 55 |

Eksterne kilder (Qt-bygningen), bundet til commit-SHA, ikke gren-navn:

| Alias | Kilde | Fuld SHA | Linjer |
|---|---|---|---|
| Q1@0f76bfed | sailfishos/qtbase `src/corelib/json/qjsonparser.cpp` (mer-5.6) | 0f76bfed2ada0ffc9a653f13c2799094e8f7dbee | 1 008 |
| Q2@0f76bfed | sailfishos/qtbase `src/corelib/json/qjson_p.h` (mer-5.6) | 0f76bfed2ada0ffc9a653f13c2799094e8f7dbee | 847 |
| Q3@0f76bfed | sailfishos/qtbase `src/corelib/json/qjsonvalue.cpp` (mer-5.6) | 0f76bfed2ada0ffc9a653f13c2799094e8f7dbee | 762 |
| Q4@0f76bfed | sailfishos/qtbase `src/corelib/json/qjsondocument.cpp` (mer-5.6) | 0f76bfed2ada0ffc9a653f13c2799094e8f7dbee | 579 |
| Q5@0f76bfed | sailfishos/qtbase `src/corelib/tools/qbytearray.cpp` (mer-5.6) | 0f76bfed2ada0ffc9a653f13c2799094e8f7dbee | — |
| Q6@0f76bfed | sailfishos/qtbase `src/corelib/tools/qtools_p.h` (mer-5.6) | 0f76bfed2ada0ffc9a653f13c2799094e8f7dbee | — |
| Q7@0f76bfed | sailfishos/qtbase `src/corelib/io/qiodevice_p.h` (mer-5.6) | 0f76bfed2ada0ffc9a653f13c2799094e8f7dbee | — |
| QT-DOC | doc.qt.io/qt-5/qjsonparseerror.html (Qt 5.15.19) | — (web) | — |

URL'er: `https://raw.githubusercontent.com/sailfishos/qtbase/0f76bfed2ada0ffc9a653f13c2799094e8f7dbee/src/corelib/json/qjsonparser.cpp` (og tilsvarende for de øvrige stier).

## 1 · Efterprøvning af F1–F8 (relæ `52b8e70e`) — alle står i blobberne

Alle otte fund blev sögt ordret i blobberne ved `0870869` og står:

- **F1 ●** — A6b-tjekket står i read-blokken, ikke i dræningsløkken, og springes over når bufferen indeholder `\n` (S1@0870869 l. 124-126, loop l. 127-134, break l. 129 uden ny måling); §5 kræver det modsatte (l. 181-182). «Linje-buffer» udefineret (l. 182); to navne på samme konstant (l. 121 `MAX_LINE` vs l. 180 `ACP_MAX_LINE_BYTES`). S3@0870869 har ingen grænse (l. 1211, 1214-1216) og ingen `QJsonParseError` overhovedet.
- **F2 ●** — samme bytes giver A6a (ét read med `\n`, l. 133), A6b (små reads, l. 126) eller intet: chunknings-invarians brudt.
- **F3 ●** — desynk-begrundelsen bærer ikke (S2@0870869 l. 53-54 siger selv, strømmen er selv-synkroniserende på `\n`); det bærende argument er hukommelsesvækst. «luk agent, stop» (S1 l. 126) uden returværdi; EOF-stien (l. 224, 229) uden grænsetjek.
- **F4 — løst denne runde:** (a) Qt's enum-navn er **`GarbageAtEnd`** (værdi 14), ikke `ExtraData`; **`ExpectingValue` findes ikke i Qt**; `DocumentTooLarge` findes (værdi 13) — Q1@0f76bfed l. 107-108, QT-DOC. S1's fejlnavne er Python `json`-navne; «ExtraData» står 7 gange i S1@0870869 (l. 141, 154, 156, 158, 164, 243, 258). (b) **●** `error->offset = json - head` (Q1@0f76bfed l. 346) — byte-offset bekræftet; QT-DOC: «offset in the input string». (c) **●** `fromJson(const QByteArray &, QJsonParseError *)` (Q4@0f76bfed) — S1 l. 92's «fromUtf8 før fromJson» er overflødig/modstridende. (d) **præciseret:** ved `GarbageAtEnd` sættes fejlen efter `eatSpace()` (Q1@0f76bfed l. 325-327) — offset peger på skrald efter whitespace, ikke «slutningen af første komplette objekt» (S1 l. 96); splitstrategien virker stadig, men sætningen skal præciseres i v2.
- **F5 ●** — tre multiplikatorer for samme konstant (S1@0870869 l. 180 «2× ~4 MB», l. 218 «8× ~2 MB»; 2×4=8, 8×2=16); worst case ~12 MB (l. 213) over grænsen vs «~4 MB» (l. 214); S2@0870869 l. 34/40/44 selvmodsigende. Fixtures målte selv: 4 587 / 275 / 237 bytes (blob `c601a620…`, `8b1763a1…`, `84b86c7f…`), alle gyldige UTF-8, alle uden trailing `\n` (sidste byte 0x73) — ingen bærer ugyldig UTF-8, så U+FFFD-stien (S1 l. 91) er utestet.
- **F6 ●** — fem statusmarkører for én beslutning (S1 l. 1, 174, 220; S2 l. 44, 100); «bevares uændret … statusnote appenderet» (S1 l. 35/38) selvmodsigende; «ingen anden fil skal åbnes sideløbende» (l. 7) mod l. 174/36; l. 14 uden A6b-undtagelse.
- **F7 ●** — strøm-rettelsen står i S1@0870869 (l. 98-111, 260-261) men ikke i S2@0870869 (l. 69, 71, 96: «ingen af de to objekter udleveres»); H1@97bd5c4 l. 238 samme modstrid. S1 l. 36 sender læseren til S2 for begrundelsen.
- **F8 ● — skærpet:** versalfølsom søgning over alle 16 filer i `src/` ved `0870869`: **ingen** nævner `QProcess`, `acp`/`vibe` eller `stdio`; eneste `readyRead` i `src/` er S3's `QNetworkReply::readyRead` (S3@0870869 l. 376-377). S3's sti er POST `https://api.mistral.ai/v1` (l. 37) med `Accept: text/event-stream` (l. 370): ingen størrelsesgrænse (l. 1211), hale uden `\n` kasseres i tavshed (l. 1086-1090 → kun komplette linjer l. 1214), `QString::fromUtf8(...).trimmed()` (l. 1215), `fromJson(jsonData.toUtf8())` **uden fejlobjekt 8 steder** (l. 1531, 1541, 1549, 1560, 1573, 1632, 1642, 1652 — målt: 8).

## 2 · Nye målinger denne runde (G1–G9)

- **G1 ●** Bygge-Qt er Sailfish **Qt5**: SP1@0870869 l. 9 `sailfishsilica-qt5 >= 0.10.9`, l. 13-17 `pkgconfig(Qt5*)`, l. 37 `%qmake5`; PR1@0870869 l. 5. Sailfish qtbase-gren `mer-5.6`, tip `0f76bfed…` (binder Q1-Q7). CI bygger mod Ubuntu `qtbase5-dev` (W1@0870869 l. 27) — **ikke** samme Qt; enum-verifikation må pege på Q1-Q7.
- **G2 ●** `main` er **ikke** beskyttet (API `branches/main` → `protected: false`). En manifest-kontrol i W1@0870869 bliver dermed en **test på PR'er, ikke en port** mod direkte push.
- **G3 ●** `DocumentTooLarge` findes på bygge-Qt (Q1@0f76bfed l. 107, 546, 765) og udløses ved `Value::MaxSize = (1<<27) - 1` = 134 217 727 B ≈ **128 MiB internt** (Q2@0f76bfed l. 600). Langt over 8 MiB — gør ikke A6 overflødig, men sætter øvre grænse for «ingen loft på komplette linjer».
- **G4 ●** Heltal: parser gemmer int kun ved |n| < 2^25, ellers double (Q1@0f76bfed l. 765); `QJsonValue::toInt` konverterer heltal-double til int (Q3@0f76bfed l. 513-518). S1 l. 81's heltals-id kan opfyldes med `toInt` — v2 skal navngive metoden.
- **G5 ●** `QByteArray`'s loft på Qt 5.6: `MaxAllocSize = (1<<30)-1` ≈ **1 GiB** (Q6@0f76bfed l. 85; `int` er 32-bit også på 64-bit); `MaxByteArraySize ≈ 1 GiB` (Q5@0f76bfed, qbytearray_p.h l. 56); over dette kastes `std::bad_alloc` (Q5 l. 1616-1617 → Q6's qBadAlloc). **Konsekvens for relæ `dd64f635`: en 4 GiB `QByteArray` er umulig på bygge-Qt — grænsen er `qBadAlloc` ved ~1 GiB, ikke OOM.** Uden A6 ender «ingen grænse» i et ufanges/crash ved ~1 GiB medmindre koden fanger `std::bad_alloc`.
- **G6 ●** Vækstlogik: `qAllocMore` = næste potens af 2 (Q5@0f76bfed l. 125-135) → allokeret ≈ op til 2× linjen (8 MiB linje → 16 MiB alloc). Ved `realloc` kan det gamle og nye blok kortvarigt være allokeret samtidigt — «intet midlertidigt dobbeltforbrug» holder kun for selve bufferen, ikke for realloc's to blokke. QIODevice's interne read-buffer: `QIODEVICE_BUFFERSIZE = 16384` (Q7@0f76bfed l. 60).
- **G7 ◐** Relæets læse-aritmetik efterprøvet aritmetisk: 16 MiB ved 4096 B blokke = 4096 ikke-tomme læsninger + 1 EOF = **4097 — passer**; ved 1 MiB blokke = 16 + 1 EOF = **17, relæet siger 18 — afvigelse 1** (måske talt en tom læsning før EOF; metode ikke angivet).
- **G8 ◐** Relæets RSS-tal (4 202 300 KiB peak ved 4 GiB) giver overhead 7,8 MiB — inden for «2-10 MiB», lineært — **aritmetisk konsistent**, men målingen kan ikke genskaber her (målmaskinen har 6 GiB RAM; og på Qt5 ville 4 GiB QByteArray crashe først, jf. G5 — så målingen må være på en anden buffer-implementering, f.eks. Python bytes eller Qt6; ikke specificeret).
- **G9 ●** `QString::fromUtf8` i Qt 5.6 erstatter ugyldig UTF-8 med U+FFFD **uden fejlsignal** — S1 l. 91's U+FFFD-logging har intet detektor; L7's pointe står.

## 3 · Svar på relæ `dd64f635`'s tre anmodninger

1. **Afløs A6 nu? — Nej (anbefaling, beslutning hos storefyr).** Forslagets (a) «ingen grænse, OOM som fysisk grænse» holder ikke på bygge-Qt: loftet er `qBadAlloc` ved ~1 GiB (G5), ikke OOM — uden A6 crasher fortolkeren (ufanget undtagelse) eller skal selv fange `bad_alloc` og beslutte noget alligevel. Forslagets (b) 16 MiB-logning er ren logning, ingen beskyttelse. A6 bør **revideres i v2**, ikke afløses: flyt hale-checket til efter dræningsløkken (post-loop-bufferen indeholder pr. definition intet `\n`), så udfaldet bliver chunknings-invariant (løser F1/F2); behold 8 MiB på halen; fjern eller behold A6a efter storefyrs valg. Pris pr. udformning er gennemgået i §5.
2. **Leveringsform:** denne granskning leveres som fil i PR #18 (denne gren) via Mistral Code; selve v2-spec'en skrives som ny fil `spec-trin-e-ndjson-parser-v2.md` når beslutningerne er truffet.
3. **Må teksten læses og kritiseres før UMÅLT-1 er målt? — Ja, og den er blevet det.** UMÅLT-1 er desuden besvaret fra kilde (ikke telefon-måling, men læsning af bygge-Qt): `GarbageAtEnd`'s offset er et **byte-offset** og peger efter `eatSpace()` (Q1@0f76bfed l. 325-327, 346) — dvs. slutningen af første objekt **plus efterfølgende whitespace**. Splitstrategien virker stadig (`linje[0:offset]` parser fint), men S1 l. 96's ordlyd skal præciseres i v2. UMÅLT-2 (om skrive-komponenten afgrænser på `\n`) står: S3@0870869 gør (l. 1214-1216); en ny QProcess-komponent skal gøre det samme — det skrives ind i v2's §2.

## 4 · Lærdommene L1–L7 (relæ `52b8e70e`) — vurdering

L1–L7 er stort set de rigtige. Præciseringer: **L1** — halvt opfyldt allerede i `0870869` (S1 er én gældende spec), men «korrektion føres ind i hver fil» er **ikke** opfyldt (F7: S2@0870869 l. 69/71/96 står uændret). **L2** — rigtig; chunkningsmatrice-fixturen findes ikke (målte selv). **L3** — rigtig; F3 præciserer at A6b's bærende argument er hukommelse, ikke desynk. **L4** — rigtig; «pr. linje, ikke pr. read» (S1 l. 181) mod algoritmens pr. read (F1). **L5** — rigtig; EOF uden kontrakt (S1 l. 229; S3 l. 1086-1090). **L6** — rigtig; §5's replay-estimat «~4 MB escaped» (S1 l. 180, 218) mangler målt oprindelse med enhed/dato/metode — L6's eget krav. **L7** — rigtig; nu med målinger: enum-navne verificeret (F4-løsning), U+FFFD uden detektor (G9), heltalspræcision har mekanisme `toInt` (G4), fjerde fixture (ugyldig UTF-8) findes ikke.

## 5 · A6 — pris pr. udformning (anbefaling til storefyr, ikke beslutning)

| Udformning | Chunknings-invariant (L2)? | Pris |
|---|---|---|
| (i) Én grænse for al akkumulation, tjek efter hver append | ● ja | A6a forsvinder; legitim 9 MiB-linje dræber sessionen; grænsen er et hårdt hukommelsesloft (skrives sådan, L4) |
| (ii) To tilstande med hårdt loft uanset `\n` | kun hvis loft tjekkes før dræn — kollapser da til (i) | A6a uopnåelig; «to tilstande» bliver til én |
| (iii) Kun halen måles — check efter dræningsløkken; komplette linjer uden loft | ● ja | A6a fjernes; én besked kan vokse til `DocumentTooLarge` (~128 MiB internt, G3) eller `qBadAlloc` (~1 GiB, G5) — v2 skal angive hvilken der først rammer og hvad der sker |

**Anbefaling: (iii)** med hale-check post-loop; 8 MiB beholdes på halen (målte caps ≤ 1 MB råt, ~4 MB escaped ◐ giver 2× margin). Betinget af målinger: største legitime linje på telefonen (3.0.2) og DocumentTooLarge/qBadAlloc adfærd. Alternativt (i) hvis en hård grænse ønskes som hukommelsesloft.

## 6 · Måleplan (CI/målmaskine vs telefon)

Afklaring målt: harnessen kører på **målmaskinen** — scripts kalder `/usr/local/bin/vibe-acp` med `PYTHONPATH=/workspace/malelab` (maaling_m00_m01.py l. 10, 13; m016-bro bro.rs l. 9-10); binæren findes der (python-shim). Telefonen kører installeret `mistral_vibe` 3.0.2 (R1@0870869 l. 6, 66). Skrynja vil spawne `vibe-acp` på telefonen — telefonens `readyRead`-portioner er dem, der tæller end-to-end.

**CI / målmaskine:** (1) chunkningsmatrice — samme byte-strøm som 1 B / 7 B / 16 KiB (QIODEVICE_BUFFERSIZE, Q7 l. 60) / 64 KiB / hel-linje portioner → identiske udfald; (2) `GarbageAtEnd`-offset-semantik mod bygge-Qt (Q1-Q4) — CI's Ubuntu-Qt må ikke bruges som kilde (G1); (3) `DocumentTooLarge`-tærskel og `qBadAlloc`-tærskel for input (G3, G5); (4) UTF-8-detektor (G9) + nyt fixture med ugyldig UTF-8; (5) heltalspræcision via `toInt` (G4): id = 0, 1, 2^25, 2^31−1.

**Telefon:** (6) største legitime wire-linje på 3.0.2 (capture over fuld session incl. replay og største tool-output) — afgør om 8 MiB hæves (S1@0870869 l. 220); (7) QProcess `readyRead`-portioner på Jolla (32-bit ARM) — device-række i matricen.

## 7 · Disposition for v2 (F8 + formkrav)

- **F8:** anbefaling — Trin E som **ny, selvstændig komponent over QProcess stdio** alene. S3's SSE-sti er en anden transport/pro protokol; hvis Trin E også skulle betjene den, kunne §3's byte-krav ikke opretholdes (S3 går `QString`, `fromJson` uden fejlobjekt 8 steder) og SSE-feltudtrækket kasserer rå NDJSON-linjer (S3 l. 1231, 1245). Med standalone kan **§2 og §3 bevares uændret**; S3's tre huller (ingen grænse, tavs hale-drop, QString) registreres som **kendt gæld** i v2's beslutningsnote. Beslutning hos storefyr.
- **Formkrav (fra `52b8e70e`, målt hvor muligt):** ny fil `spec-trin-e-ndjson-parser-v2.md`; v1's fil bevares uden retroaktive ændringer og **uden statusnote** (F6b); ingen fil bærer sin egen blob-SHA — bagudrettet ophavslinje med forrige revisions fulde SHA (`0870869fcc5f6209aebbefad31fa1a2dfa269456` for v2), skrevet af det følgende commit; én statusmarkør pr. beslutning med beslutningstager + dato; manifest `spec-manifest.json` (alias, sti, commit-SHA, blob-SHA, bytes, linjer, sektionsoversigt pr. normativ fil, bekræftelses-revision; sektionsoversigt aldrig håndskrevet, aldrig inde i filen selv; manifest uden egen hash); kontrol i W1@0870869 fejler ikke-nul ved afvigelse — **bemærk G2: `main` er ikke beskyttet, så kontrollen er en test på PR'er, ikke en port**; alle enum/metode-navne verificeret mod bygge-Qt med version + URL: Sailfish OS 4.5.0.18 (W1@0870869 l. 10), qtbase `mer-5.6` @ `0f76bfed2ada0ffc9a653f13c2799094e8f7dbee`, sekundært doc.qt.io/qt-5/qjsonparseerror.html (5.15.19). Enum-korrektion til v2: `ExtraData` → **`GarbageAtEnd`**; `ExpectingValue` findes ikke i Qt.

## 8 · Åbent hos storefyr (beslutninger, ikke truffet her)

1. A6-udformning: (i), (ii) eller (iii) — anbefaling (iii), jf. §5.
2. F8-scope: Trin E kun stdio (anbefalet) eller også SSE-stien.
3. Om 8 MiB hæves, afventer telefon-måling (punkt 6).
4. Relæ-nummerering: per runde eller per leverance (S1 l. 3 og S2 l. 104 angiver relæ `83268bd1` som runde 12; ikke målt hvad der tæller).

**Status ◐:** Granskning færdig; F1-F8 efterprøvet ● mod `0870869`; G1-G9 målt/læst denne runde; beslutninger ovenfor afventer storefyr. Ingen fil i denne runde er blevet ændret bagudrettet; v1-spec'en står som gældende indtil v2 vedtages.
