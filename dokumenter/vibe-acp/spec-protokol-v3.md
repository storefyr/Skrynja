# Skrynja — protokol-specifikation v3 (ACP-klient over stdio)

**Status [B]:** Dette dokument er gældende protokol-specifikation for Skrynjas
ACP-klient. Det erklærer sin egen gyldighed; det udtaler sig ikke om noget andet
dokuments status. Beslutningstager: storefyr, 2026-10-29.
**Målgruppe:** den agent, der implementerer Skrynja.
**Grundlag:** ACP v1 [K], JSON-RPC 2.0 [K], Qt (mer-5.6-grenen, [M] nedenfor),
`vibe-acp` 3.0.2 med `agent-client-protocol==0.11.0` [M].
**Erstatter:** v2. Ændringerne er angivet i M.8 og i §12.

**Praktisk prøve (gælder hver sætning i dette dokument):** Kan en læser, der
aldrig har set noget andet dokument i dette projekt, forstå sætningen på dens
egne præmisser? Kan ikke, skal den skrives om eller ud.

---

## M Mærkning af påstande

### M.1 Formål

Mærkningen angiver hæftelse, ikke sikkerhedsgrad. En markør svarer på, hvad
læseren kan gøre ved påstanden: efterprøve kravet, slå kilden op, afvise
slutningen, se hvem der bestemte det, eller se at punktet er åbent. Den siger
intet om, hvorvidt påstanden er sand.

### M.2 De seks former

| Form | Betydning | Læserens mulighed |
|---|---|---|
| `[K]` | normativt krav fra ACP eller JSON-RPC | efterprøve mod specifikationen |
| `[M]` | bundet til målt eller læst kilde i Bilag D | slå kilden op |
| `[B]` | vedtagelse; beslutningstager og dato skal stå i samme sætning eller række | se hvem der bestemte det, og hvornår |
| `[S]` | udledt eller foreslået | afvise slutningen |
| `[A]` | ikke besluttet | se at punktet er åbent |
| `[V]` | standardværdi, kun i samme sætning som `[A]` | se hvilken værdi der blev antaget |

Der findes ingen symbolformer. `[K] [M] [B] [S] [A] [V]` er den eneste form.

Hvor en `[B]`-række ikke selv nævner en dato, gælder dokumentets dato,
2026-10-29. Datoen er ikke målt pr. række.

### M.3 Skrevne former

1. Markøren skrives som et felt i kantede parenteser: `[M]`, `[A110]`.
2. Forkortelser efter en markør er ikke tilladte. `○d` findes ikke i v3; det
   var i v2 en `○` efterfulgt af et bogstav uden defineret betydning.
3. Vinklede parenteser må ikke anvendes i filen, hverken som markør, som note
   eller som pladsholder. `⟨110⟩` skrives `[A110]`; en note skrives som
   almindelig tekst i parentes eller på særskilt linje; en pladsholder skrives
   med krøllede parenteser, fx `{org}`.
4. `[V]` må kun stå i samme sætning som et `[A]`.
5. `[B]` må ikke stå uden beslutningstager. En vedtagelse uden navn er en `[S]`.
6. Ukendte `[…]`-former er fejl, ikke plads til fortolkning.

### M.4 Tilstand og reference

| Form | Hvor | Funktion |
|---|---|---|
| `[A]` | statuskolonne | tilstand: punktet er ikke afgjort |
| `[A<n>]` | løbende tekst, tabeller uden statuskolonne | reference til punkt n |

Referencer må ikke tælles som tilstande. Et punkt kan være refereret vilkårligt
mange gange og have præcis én tilstand.

Statuskolonnen i §12 er ikke ensartet: den kan rumme `[A]`, en afgørelse i
prosa eller ordet »nej« for reserverede numre. Et punkt uden `[A]` er derfor
ikke et punkt uden status.

### M.5 Placering

| Flade | Markør | Begrundelse |
|---|---|---|
| prosa | ja | her skimtes der; markøren bærer hæftelsen |
| tabel med markørkolonne | ja, egen kolonne | kolonnen er markøren |
| tabel uden markørkolonne | ja, i første celle | [S] antagelse; ikke målt i nogen tabel |
| overskrift | nej | overskriften er selv markøren |
| commit, indeks, søgelinje | påkrævet | ingen læser til at bruge andet |

### M.6 Hvad `[M]` ikke garanterer

`[M]` betyder »bundet til en af kilderne i Bilag D«, ikke »efterprøvet for
denne påstand«. Rækken skal navngive kilden. Henvisning til en flydende gren
uden commit eller dato er ikke en binding.

Dækningen i Bilag D er ikke ens for alle kilder: dato og maskine gælder
leverancen som interval, mens metode er beskrevet for agentinstallationen
alene. Det fremgår af fortegnelsen, ikke af markøren.

### M.7 Kontrol

| Kontrol | Kan kontrolleres | I CI |
|---|---|---|
| ukendt `[…]`-form | ja | ja |
| vinklet parentes i filen | ja | ja |
| `[A]` eller `[A<n>]` uden række i §12 | ja | ja |
| `[V]` uden `[A]` i samme sætning | ja | nej — sætningsopdeling fejler på `0.11.0`, `§9.2e` |
| `[B]` uden beslutningstager | ja | nej — kræver navneliste |
| forkortelse efter markør | ja | nej — kræver ordliste |
| om `[S]` er korrekt anvendt | nej | nej |
| om kilden understøtter påstanden | nej | nej |

Kontrollen efterprøver formen, ikke sandheden. Grøn kontrol er ikke et
vidnesbyrd om, at teksten er rigtig.

Indføres kontrollen på hele filen, skal den først gælde ændrede linjer.

### M.8 Ændringer fra v2

| Emne | v2 | v3 |
|---|---|---|
| symboler | `■ ● ○ ⟨åben⟩ ⟨n⟩ ○v ○d` | `[K] [M] [B] [S] [A] [A<n>] [V]` |
| vedtagelser | markeret `●` | `[B]` med beslutningstager |
| `○d` | elleve steder, uden definition | udgår; skrives `[S]` |
| vinklede parenteser | markør, note og pladsholder | forbudt i alle tre funktioner |
| åbent punkt 95 | både linjeafslutning og tomme linjer | 95 = tomme linjer; linjeafslutning = 126 |
| intern kode 1002 | klient-syntetisk timeout med `−33000` | udgået; hul i 1-blokken |
| intern kode 3004 | kun omtalt i §9.2e | egen række N1 i §6.2 |
| Bilag A | linjenumre i kildefiler | bevaret; sted og commit angivet i metodeafsnittet |

---

## §0 Formål og forudsætninger

**§0.1 Forudsætninger**

| Forhold | Værdi | Status |
|---|---|---|
| Skrynja er klienten (L1), agenten er underordnet program | stdio-transport | [K] ACP |
| Enhed | Jolla Phone 2026, aarch64, Sailfish OS 5 | [M] målt på enheden 2026-10-26 til 2026-10-29 (Bilag D) |
| Værktøjskasse | Qt 5 (Sailfish-pakker `qt5-*`, `sailfishsilica-qt5`), Silica | [M] rpm-spec læst 2026-10-29; præcis qtbase-version [A110] åben |
| Agent | `vibe-acp` 3.0.2, sti `/home/defaultuser/.local/bin/vibe-acp` | [M] målt på enheden 2026-10-29 (Bilag D) |
| Agentens ACP-bibliotek | `agent-client-protocol==0.11.0` (ren Python-pakke, wheel `agent_client_protocol-0.11.0-py3-none-any.whl`) | [M] PyPI-metadata + wheel læst 2026-10-29 (Bilag D) |
| Kodefremstilling | genereret, ikke håndskrevet | [M] commit-metadata i dette repo |

**§0.2 Omfang.** Læsning af byte fra agentens standardud, afgrænsning af linjer,
fortolkning af JSON, hvad klienten svarer, og hvornår forbindelsen afsluttes.
ACP's sessionslag, filadgang, terminaler og indholdsblokke er **ikke** omfattet
af dette dokument.

**§0.3 Hård regel.** Ingen regel i dette dokument kræver, at klienten sender en
kode, som ACP eller JSON-RPC ikke definerer. JSON-RPC reserverer −32768…−32000
og tillader applikationsdefinerede koder uden for det område, men det er en
tilladelse, ikke en opfordring ([K] jsonrpc.org/specification, afsnit 5.1).
Skrynja bruger den ikke.

---

## §1 Terminologi

| Begreb | Betydning |
|---|---|
| **linje** | byte fra første byte efter en `0x0A` til og med næste `0x0A` |
| **bruttolinje** | linjens byte uden den afsluttende `0x0A` |
| **n** | bruttolinjens længde i byte |
| **dokument** | det JSON-dokument, en bruttolinje indeholder |
| **hale** | byte akkumuleret ved strømmens slutning uden afsluttende `0x0A` |
| **spor** | klientens lokale log; går aldrig på tråden |
| **L-kode** | intern hændelseskode (§6.5, Bilag C); aldrig en trådkode |

---

## §2 Meddelelseslaget

**§2.1 Afgrænsning.** ACP afgrænser meddelelser med `\n` og forbyder indlejrede
linjeskift. Klienten læser byte og udtrækker en linje ved hver `0x0A`.
[K] ACP-transport; SDK 0.11.0's stdio-transport læser med `readline()` og
skriver newline-afsluttede frames ([M] `acp/connection.py` docstring
«newline-delimited JSON frames», `acp/stdio.py` `sys.stdin.buffer.readline()`,
læst 2026-10-29).

**§2.2 Linjeafslutning.** Et `\r` før `0x0A` er del af bruttolinjen og fjernes
ikke. En rå `0x0D` uden for en JSON-streng er ugyldig JSON og giver
fortolkningsfejl. [A126] Standardværdi [V]: `\r\n` accepteres **ikke** som
linjeafslutning. Kan ændres ved kompilering, ikke ved kørsel.

**§2.3 BOM.** Qt's fortolker spiser selv `EF BB BF` i starten (`Parser::eatBOM()`).
Klienten fjerner derfor **ikke** BOM, da det ville forskyde positionerne i §5.3
i forhold til Qt's egne. [M] læst i qtbase-kilde (Bilag A, A14).

**§2.4 Tomme linjer.** [A95] Standardværdi [V]: bruttolinje med n = 0 ignoreres
uden spor. Kan slås fra ved kompilering, ikke ved kørsel.

**§2.5 Tegnsæt.** JSON-RPC-meddelelser er UTF-8 ([K] jsonrpc.org/specification,
§1 «Overview»). Klienten validerer ikke selv; Qt rapporterer `IllegalUTF8String`.
Klienten må ikke »retningskorrigere« ugyldige byte.

---

## §3 Læsning, buffer, hale

**§3.1 Buffermodel.** Én buffer pr. forbindelse. Byte lægges til i ankomstorden.
Bufferen tømmes til og med den `0x0A`, der afslutter linjen.

**§3.2 Udtrækning.** Når `0x0A` er set, er linjen komplet og overgives til §4
og §5.

**§3.3 Hale.** Byte ved strømmens slutning uden `0x0A`:

1. fortolkes som et dokument efter §5,
2. leveres, hvis fortolkningen lykkes,
3. registreres som »afsluttet af EOF, ikke af `0x0A`«,
4. besvares **ikke**.

Trin 1 er ikke høflighed: en agent, der skriver sit sidste svar uden afsluttende
linjeskift, har stadig sendt et svar [S]. Trin 4 følger af, at der ikke findes en
linje at besvare.

**§3.4 Halens øvre grænse.** Halen kan ikke vokse forbi §4.2's loft. Uden loft
findes en tilstand, hvor klienten hverken har svaret, sporet eller en forbindelse
[S].

---

## §4 Størrelsesgrænser

**§4.1 Fire lag**

| Lag | Grænse | Art |
|---|---|---|
| ACP | ingen størrelsesgrænse; kun `\n` og forbud mod indlejrede linjeskift | [K] protokol |
| Skrynjas linjeloft | 66 060 288 byte, §4.2 | [B] beslutningstager storefyr 2026-10-29 |
| Qts dokumentgrænse | `Value::MaxSize = (1<<27) - 1` = 134 217 727 byte **binært** | [M] læst i qtbase-kilde (Bilag A, A6) |
| Qts indlejringsgrænse | `nestingLimit = 1024` | [M] læst i qtbase-kilde (Bilag A, A5) |

**§4.2 Linjeloft**

> Loftet er **66 060 288 byte** (63 MiB) på bruttolinjens længde.
>
> Loftet er klientens, ikke protokollens.
>
> Grænsen er aritmetisk begrundet: Qts binære form kan være op til ca. dobbelt
> så stor som teksten, og `Value::MaxSize` er 134 217 727 byte.
> 66 060 288 × 2 = 132 120 576 < 134 217 727. Marginen er 2 097 151 byte.
>
> **Loftet er ikke en garanti.** `DocumentTooLarge` kan indtræde under loftet,
> fordi fordoblingsfaktoren ikke er målt for alle dokumenttyper. Den håndteres
> som L2 i §6.2.

**§4.3 Qts kontrolpunkter.** `current - baseOffset >= Value::MaxSize` kontrolleres
ved hvert matrixelement, hver streng, hver indlejret matrix, hvert indlejret
objekt og ved tal gemt som double. `baseOffset` er den **omsluttende** beholder,
ikke dokumentets begyndelse. Grænsen gælder den binære form, ikke tekstlængden.
[M] (Bilag A, A6).

**§4.4 Indlejring.** `parseObject` og `parseArray` afviser ved
`++nestingLevel > nestingLimit` med `DeepNesting`; loftet er 1024. Intet
linjeloft erstatter det. [M] (Bilag A, A5).

**§4.5 Hukommelsesbudget.** Ved 63 MiB kan klienten samtidig holde bruttolinjen
(~66 MiB), den binære form (op til ~132 MiB) og dokumentet oven på: ~222 MiB [S]
(66 MiB bruttolinje + 34 MiB foregående buffer under omallokering + 132 MiB
binær form, jf. A15/A16). Belastningen skal stå i sporudskriften ved L1, så en
senere hukommelsesfejl kan henføres til den rigtige årsag.

**§4.5a Forholdet til A6 i PR-beskrivelsen.** A6 i PR-beskrivelsen angiver 8 MiB
pr. linje og lukning ved hale uden `0x0A`. Denne specifikation hæver loftet til
63 MiB (§4.2) og lader forbindelsen bestå ved hale (§6.2, L1). Ændringen er
tilsigtet [S]: 8 MiB ville afvise dokumenter, som Qts `Value::MaxSize` ellers
rummer, og lukning ved hale koster en hel session for én fejl. A6 i Bilag A er en
anden post (dokumentgrænsen).

**§4.6 Utæthed i Qt.** `Parser::parse` allokerer med `malloc` og skriver derefter
headeren uden at kontrollere, om `malloc` returnerede nul. [M] (Bilag A, A11).
En meget stor linje på en presset enhed kan derfor give nul-pointer-dereference
i stedet for en `QJsonParseError`. Klienten kan ikke rette det; den kan holde
linjen under loftet [S].

**§4.7 Den måling, der mangler.** Om fordoblingen holder ved 63 MiB, er ikke
målt. Bilag B viser måleprogrammet. [A115]

---

## §5 Fortolkning

**§5.1 Tolerancen.** [A96] Standardværdi [V]: **fra**. Klienten fortolker én
bruttolinje som præcis ét dokument. Kan slås til ved kompilering.

**§5.2 Klæbede dokumenter.** Er tolerancen slået til, og rapporterer Qt
`GarbageAtEnd`, fortolkes resten af linjen som et nyt dokument fra
fejlpositionen, indtil linjen er opbrugt eller en anden fejl indtræder.

**§5.3 Position og snit.** `QJsonParseError::offset` er et **byte**-offset fra
bruttolinjens begyndelse (`error->offset = json - head`). [M] (Bilag A, A7).

Ved en uafsluttet streng løber `parseString` til `json >= end`, rykker én frem
og rapporterer først derefter fejlen: offset kan blive **n + 1**. Klienten skal
derfor **klemme** positionen til `[0, n]` før ethvert snit. Uden klemning kan
§5.2 læse ud over bruttolinjens slutning [S]. (Bilag A, A8).

**§5.4 Hvad Qt afviser ved toppen.** Kun `[` og `{` accepteres som første
strukturtegn; alt andet giver `IllegalValue`. [M] (Bilag A, A1, A2). Et nøgent
tal, en streng, `true`, `false` eller `null` som helt dokument giver dermed
`IllegalValue`, **ikke** `TerminationByNumber`. Sidstnævnte opstår kun inde i en
struktur, når input ender midt i et tal (Bilag A, A3).

**§5.5 Skalarlyprøven.** Fremgangsmåde [V]:

1. Fortolk bruttolinjen som den er.
2. Resultatet er et objekt → §6.
3. Resultatet er en matrix → §8.
4. Fortolkningen fejlede med `IllegalValue`, og linjen begynder (efter BOM og
   blanktegn) med noget andet end `[` eller `{` → **indpak i `[` … `]` og
   fortolk igen**.
5. Indpakningen lykkes med præcis ét element mellem `[` og `]`, og elementet er
   en skalarværdi → gyldig, men ikke-meddelelsesformet JSON-værdi → −32600. Er
   der flere elementer eller et objekt/en matrix som element, er linjen ikke en
   skalarlyprøve og afvises efter §5.1.
6. Indpakningen mislykkes → ikke gyldig JSON → −32700.

Indpakningen er nødvendig, fordi Qt's fortolker ikke har begrebet
topniveau-skalar [A97].

**§5.6 Positionsforskydning.** Efter vellykket indpakning er positioner fra den
anden fortolkning forskudt med +1. De bruges kun til spor, aldrig til snit i
bruttolinjen.

**§5.7 Forbud.** Klienten må ikke reparere en linje før fortolkning (undtagen
§2.3 og §5.5), ikke fjerne eller tilføje byte for at »gøre JSON'en gyldig«, og
ikke acceptere et dokument, som Qt afviste, ved egen fortolkning. **Én
fortolker.** [K]

---

## §6 Fejl og svar

**§6.1 Trådkoder.** Klienten sender kun de koder, der står i tabellen. Koder
klienten alene modtager, er noteret umiddelbart under den.

| Kode | Betydning | Status |
|---|---|---|
| −32700 | Parse error | [K] JSON-RPC |
| −32600 | Invalid Request | [K] JSON-RPC |
| −32601 | Method not found | [K] JSON-RPC |
| −32602 | Invalid params | [K] JSON-RPC |
| −32603 | Internal error | [K] JSON-RPC |

−32800 er ikke i SDK 0.11.0-wheelen og sendes ikke i v3.
−32002 findes i SDK 0.11.0 (`RequestError.resource_not_found`). Klienten kan
modtage den, men sender den ikke i dette omfang: ingen tilstand i §6.2 fremkalder
den.
−32768…−32000 er reserveret [K]. Skrynja har ingen egen trådkode.
[A123]: tabellen skal efterprøves igen, hvis agentens SDK-version ændres (§9.2d).

**§6.2 Hændelsestabel**

| # | Hændelse | Svar | Spor | Forbindelse |
|---|---|---|---|---|
| **N1** | agentstien ikke nåbar (§9.2e) | intet | 3004 | startes ikke |
| **L1** | bruttolinje > §4.2's loft | intet | 1001 | består; byte kasseres til næste `0x0A` |
| **L2** | Qt: `DocumentTooLarge` | intet | 2002 | består |
| **L3** | Qt: `DeepNesting` | −32700, `id: null` | 2003 | består |
| **P1** | ikke gyldig JSON (øvrige `ParseError`) | −32700, `id: null` | 2001 | består |
| **P2** | gyldigt JSON, ikke et objekt (efter §5.5) | −32600, `id: null` | 3001 | består |
| **P3** | objekt uden `jsonrpc: "2.0"` | −32600, `id: null` | 3001 | består |
| **P4** | svar med ukendt `id` | intet | 3002 | består |
| **P5** | ukendt metode | −32601, samme `id` | 3003 | består |
| **C1** | EOF med udestående anmodninger | intet | 4001 | lukkes |
| **C2** | agenten afsluttede med status ≠ 0 | intet | 4002 | lukkes |

**§6.3 Begrundelser**

*P1.* `id: null`, fordi id'et ikke kan kendes: »If there was an error in
detecting the id in the Request object (e.g. Parse error/Invalid Request), it
MUST be Null« ([K] jsonrpc.org/specification §5). Specifikationens eget eksempel
viser det samme (§7, «rpc call with invalid JSON» → `"id": null`).

*N1.* Nåbarhedsprøven sker før processen startes, så der er ingen forbindelse at
svare på og intet id at knytte svaret til. At registrere den som egen hændelse
holder den adskilt fra C1 og C2, der begge forudsætter, at der *var* en
forbindelse [S].

*L1 og L2.* Intet svar. Meddelelsen er ikke modtaget i sin helhed, så det er ikke
kendt, om den var en anmodning, og hvilket id den havde. Et svar med `id: null`
ville være formelt i orden, men ville besvare en meddelelse, afsenderen muligvis
aldrig færdiggjorde. At tie og registrere påstår intet om det ukendte [S].

*L3.* `DeepNesting` er en egenskab ved dokumentet, ikke ved dets størrelse. Linjen
**er** modtaget i sin helhed. Derfor svares der — med −32700, fordi dokumentet
ikke er fortolkeligt for klienten [A117] (−32600 er alternativet; valget er ikke
bundet til nogen kilde).

*P4.* Et svar uden modpart er ikke en protokolfejl hos klienten, men et tegn på,
at parterne er ude af trit. Det spores og kasseres ([K] jsonrpc.org §5: svarets
id skal svare til anmodningens).

**§6.4 Rækkefølge.** Fortolkning før semantik. Klienten må ikke læse `jsonrpc` ud
af en linje, den ikke har fortolket.

**§6.5 Interne koder.** Positive heltal, blokopdelt (Bilag C). De har deres egen
type i koden, kan ikke tildeles `error.code` uden eksplicit konvertering, skrives
til sporet med linjens længde, position og tidspunkt, går **aldrig** på tråden —
heller ikke i `error.data` — og ændres ikke, når §4.2's loft ændres.

1000 er reserveret og ubrugt: en fejlagtig standardværdi skal give et
genkendeligt spor, ikke en tavs succes [S]. Et svar fra agenten med koden 1001 er
en trådkode og skal behandles som sådan; de to talrum må ikke sammenlignes.

---

## §7 Livscyklus

**§7.1 Start.** Agenten startes som underordnet program (§9.2). [K] ACP.

**§7.2 Standardfejl.** Opsnappes, lægges i sporet med eget tidsstempel, fortolkes
ikke som meddelelser.

**§7.3 Standardud er rent.** Agenten må ikke skrive andet end ACP-meddelelser til
standardud. Klienten kontrollerer ikke proaktivt, men en linje, der ikke kan
fortolkes, er P1 — uanset om årsagen er agent eller klient.

**§7.4 Lukning.** Klienten lukker agentens standardind og afventer afslutning.
Udestående anmodninger besvares ikke; de markeres internt, så den del af
grænsefladen, der ventede, får at vide, at svaret ikke kommer. Hvilken kode der
vises i grænsefladen, er ikke en trådkode.

**§7.5 Tvungen afslutning.** Fristen er en konfigurationsværdi, ikke et tal i
dette dokument. SIGTERM, derefter SIGKILL. Signalet skal ramme agenten, ikke et
mellemled (§9.2c).

---

## §8 Batch

En bruttolinje, hvis dokument er en matrix, er et batch [K] (jsonrpc.org §6).

| Tilfælde | Klientens svar |
|---|---|
| tom matrix `[]` | **ét** objekt, −32600, `id: null` [K] (§7 eksempel «empty Array») |
| matrix uden gyldige anmodninger | ét svar **pr. element**, −32600, `id: null` [K] (§7 eksempel `[1,2,3]`) |
| kun notifikationer | **intet** svar [K] (§7 eksempel «all notifications») |
| blandet | ét svar pr. anmodning, intet for notifikationer [K] (§6, §7) |

Batch-svaret er selv en matrix og skal skrives på **én** linje, fordi ACP forbyder
indlejrede linjeskift (§2.1).

**§8.1 Forbehold.** [A116] Det er ikke målt, om ACP's transport tillader batch.
Standardværdi [V]: batch understøttes som ovenfor. En alpha må gerne svare −32600
på batch, hvis den logger det som en afvigelse.

---

## §9 Transport og agent

**§9.1 Transport.** stdio [K]. Agenten er underordnet program [K]. Klienten
skriver kun gyldige ACP-meddelelser til agentens standardind.

### §9.2 Agentkonfiguration — ni felter

| # | Felt | Værdi | Status |
|---|---|---|---|
| 1 | programsti | `/home/defaultuser/.local/bin/vibe-acp` | [M] målt på enheden 2026-10-29 (Bilag D) |
| 2 | argumenter | **tom liste** | [M] |
| 3 | arbejdsmappe | projektets rodmappe, sat eksplicit før start | [B] storefyr 2026-10-29 |
| 4 | miljøvariable | `MISTRAL_API_KEY` arves eller sættes | [M] krævet mål/API-nøgle, målt runde 1 |
| 5 | opsætningsvej | `uv tool install`, ikke `uvx` | [B] storefyr 2026-10-29 (§9.2c) |
| 6 | binærnavn og version | `vibe-acp` **3.0.2** | [M] målt på enheden (pip show, Bilag D) |
| 7 | standardfejl | opsnappes, §7.2 | [B] storefyr 2026-10-29 |
| 8 | standardind | lukkes ved nedlukning, §7.4 | [K] |
| 9 | frist for afslutning | konfigurationsværdi | [S] |

**§9.2a Forbudte argumenter.** Klienten må **ikke** sende `--setup`. I
`vibe/acp/entrypoint.py` på tag `v3.0.2` (læst 2026-10-29 via GitHub API) kører
flaget `run_onboarding(...)` og derefter `sys.exit(0)` — afgangskode 0, altså
udseendemæssigt identisk med en ren nedlukning, som C2 i §6.2 ellers ville
klassificere som normal. [M]

**§9.2b Arbejdsmappe.** Agenten accepterer intet mappeflag. Klienten sætter
arbejdsmappen i spawn-kaldet, før processen startes.

**§9.2c Ét procesled.** Klienten starter eksekverbarheden direkte. `uvx` må ikke
bruges som spawn-form: den indfører uv som ekstra led, så §7.5's signal rammer det
forkerte led [S].

**§9.2d Versionsbinding.** §6 og §8 er bundet til `vibe-acp` 3.0.2 og
`agent-client-protocol` 0.11.0 ([M] PyPI, Bilag D). Ændres en af de to, skal §6 og
§8 efterprøves igen. [A124]: om bindingen skal være præcis 3.0.2 eller ≥3.0.2, er
ikke besluttet.

**§9.2e Indespærring.** Sailjail giver skriveadgang til præcis tre stier under
hjemmemappen — `$HOME/.local/share/{org}/{app}`, `$HOME/.cache/{org}/{app}`,
`$HOME/.config/{org}/{app}` — og hvis appen ikke har tilladelse til en mappe, ser
den kun en tom, skrivebeskyttet mappe der ([M] sailjail-permissions README, læst
2026-10-29, Bilag D). Tilladelseslisten indeholder ingen tilladelse, der giver
»kør en vilkårlig binær i hjemmemappen«; den nærmeste er `AppLaunch`
(systemd-tjenester), og `Compatibility` frarådes. Standardprofilen forudsætter
desuden præcis én binær (Exec i desktop-filen). Følgelig [S]: en pakket,
indespærret Skrynja kan sandsynligvis ikke starte
`/home/defaultuser/.local/bin/vibe-acp`. **Klienten skal måle stiens nåbarhed
(`QFileInfo::exists`) før spawn og logge resultatet som hændelse N1 (3004).**
[A122] åben indtil målt.

**§9.2f Netværk.** Agenten taler med en fjerntjeneste. Appen skal have
`Internet`-tilladelsen ([M] sailjail-permissions README). Uden den er klienten en
parser uden modpart.

**§9.3 Miljø.** Skrynja er en Silica-app. [A110] Qt-bindingen (præcis
qtbase-version) skal bekræftes med `ldd` på enheden eller mod rpm-spec, før
Bilag A's versionsangivelse er endelig.

---

## §10 Resumé

| # | Regel | Værdi | Status |
|---|---|---|---|
| 1 | Afgrænsning | `0x0A` | [K] |
| 2 | Indlejrede linjeskift | forbudt | [K] |
| 3 | `\r\n` | ikke accepteret | [V] [A126] |
| 4 | BOM | overlades til Qt | [M] Bilag A |
| 5 | Tom linje | ignoreres; kontakt ved kompilering | [V] [A95] |
| 6 | Linjeloft | 66 060 288 byte | [B] storefyr 2026-10-29 |
| 7 | Qts dokumentgrænse | `(1<<27) - 1` binært | [M] Bilag A |
| 8 | Indlejringsgrænse | 1024 | [M] Bilag A |
| 9 | Tolerance | fra | [V] [A96] |
| 10 | Skalarlyprøve | indpakning i `[` … `]` | [V] [A97] |
| 11 | Position | byte, klem til `[0, n]` | [M] Bilag A |
| 12 | L1-svar | intet | [S] |
| 13 | P1-svar | −32700, `id: null` | [K] JSON-RPC |
| 14 | Batch | JSON-RPC's regler | [V] [A116] |
| 15 | Interne koder | 1001 = L1, blokopdelt | [B] storefyr 2026-10-29 |
| 16 | Egne trådkoder | ingen | [K] JSON-RPC |
| 17 | Agentsti | `/home/defaultuser/.local/bin/vibe-acp` | [M] målt 2026-10-29 |
| 18 | Agentversion | 3.0.2 / ACP 0.11.0 | [M] målt 2026-10-29 |

---

## §11 Miljø og bygning

| Forhold | Værdi | Status |
|---|---|---|
| Apptype | Silica | [B] storefyr 2026-10-29 |
| Qt | Qt 5 (Sailfish), præcis version [A110] | [M] rpm-spec |
| Fortolker | `QJsonDocument::fromJson` | [M] |
| Antal fortolkere | én | [K] §5.7 |
| Sprog i meddelelser | UTF-8 | [K] |

---

## §12 Åbne punkter

| # | Punkt | Status |
|---|---|---|
| 95 | Tomme linjer | [A], [V]: ignoreres, ved kompilering |
| 96 | Tolerance som standard | [A], [V]: fra |
| 97 | Skalarlyprøven erstatter fallback | [A], [V]: ja, §5.6 kun til spor |
| 110 | Præcis Qt-binding (`ldd`/spec) | [A], blokerer Bilag A's endelige versionsangivelse |
| 115 | Holder fordoblingen ved 63 MiB | [A], Bilag B |
| 116 | Accepterer ACP batch | [A], [V]: ja |
| 117 | L3: −32700 eller −32600 | [A], [V]: −32700 |
| 118 | Agentens faktiske fejlkoder på wire | [A], kræver kørsel |
| 122 | Er agentstien nåbar fra den indespærrede app? | [A] — **alpha-blokerende**; mål med N1 (3004) |
| 123 | §6.1 og §8 efterprøvet mod SDK-versionen | [A] ved versionsskift (§9.2d) |
| 124 | Binde til præcis 3.0.2 eller ≥3.0.2 | [A], beslutning hos storefyr |
| 125 | 3004 som egen L-kode for nåbarhed | medtaget i Bilag C og §6.2 som N1; sendes ikke (§6.1) |
| 126 | `\r\n` som linjeafslutning | [A], [V]: accepteres ikke. Nyt i v3; nummeret er tildelt ved redigeringen og skal bekræftes af storefyr |

**Ikke målt:** punkt 122 (nåbarhed fra indespærret app), Bilag B (fordobling ved
63 MiB), punkt 118 (agentens faktiske fejlkoder på wire), [A110] (præcis
qtbase-version på enheden).

---

## Bilag A — Qt-kildegrundlag (læst i denne leverance)

**Metode:** kildefiler læst fra `sailfishos/qtbase`, gren `mer-5.6`, commit
`0f76bfed2ada0ffc9a653f13c2799094e8f7dbee`, hentet 2026-10-29 på målmaskinen
(sparse clone). URL:
`https://raw.githubusercontent.com/sailfishos/qtbase/0f76bfed2ada0ffc9a653f13c2799094e8f7dbee/src/corelib/json/qjsonparser.cpp`
(og `qjson_p.h`, `qjsonvalue.cpp`, `qjsondocument.cpp`, `qbytearray.cpp`,
`qtools_p.h`, `io/qiodevice_p.h`). Sailfish OS 4.5.0.18 bygger mod denne gren
(CI's `SAILFISH_RELEASE: 4.5.0.18`). En kørselsmåling på selve enheden mangler
stadig [A110].

Linjenumrene nedenfor gælder den nævnte commit. De er ikke stabile mellem
forskellige versioner af filen og skal måles igen, hvis commit'en ændres.

| # | Forhold | Fund i kilden | Linje i kilden |
|---|---|---|---|
| A1 | Topniveau | kun `[` og `{`; ellers `IllegalValue` | qjsonparser.cpp 314-321 |
| A2 | Nøgent tal/streng/bool/null som dokument | `IllegalValue` (ikke `TerminationByNumber`) | qjsonparser.cpp 321, 599 |
| A3 | Tal ved strukturens slutning | `TerminationByNumber` når input ender i tal | qjsonparser.cpp 755 |
| A4 | Affald efter dokumentet | `GarbageAtEnd`, sat efter `eatSpace()` | qjsonparser.cpp 325-327 |
| A5 | Indlejringsgrænse | `nestingLimit = 1024`, `DeepNesting` | qjsonparser.cpp 55, 382, 523 |
| A6 | Dokumentgrænse | `Value::MaxSize = (1<<27) - 1`, tjek ved `current - baseOffset >= MaxSize` | qjson_p.h 600; qjsonparser.cpp 546 med flere |
| A7 | Offset-enhed | `error->offset = json - head` — byte-offset | qjsonparser.cpp 346 |
| A8 | Offset ved uafsluttet streng | `++json` før fejlen → kan blive n+1 | qjsonparser.cpp 938-940, 992-994 |
| A9 | Komprimeret heltal | `int` når `\|n\| < 2^25`, ellers double; `QJsonValue::toInt` konverterer heltal-double | qjsonparser.cpp 765; qjsonvalue.cpp 513-518 |
| A10 | Latin1-grænse | streng over 0x8000 byte går til UTF-16 | qjsonparser.cpp 926 |
| A11 | Mislykket allokering | `data = (char *)malloc(dataLength)` uden nul-tjek, derefter skrivning til headeren | qjsonparser.cpp 301-303 |
| A12 | Størrelsestabel | Latin1: 2+len; UTF-16: 4+2·len; Array: 12+4·len; Object: 12+8·len | qjson_p.h 74-84 |
| A13 | Binær form vs tekst | kommentar: «slightly bigger binary size» end kompakt tekst | qjson_p.h 92-94 |
| A14 | BOM | `eatBOM()` spiser `EF BB BF` | qjsonparser.cpp 242-248, 310 |
| A15 | QByteArray-loft | `MaxAllocSize = (1<<30)-1` (~1 GiB, 32-bit int også på 64-bit); over → `qBadAlloc()` (std::bad_alloc) | qtools_p.h 85; qbytearray.cpp 1616-1617 |
| A16 | QByteArray-vækst | `qAllocMore` = næste potens af 2; `QIODevice` intern buffer 16 384 byte | qbytearray.cpp 125-135; qiodevice_p.h 60 |

Enum-navne i Qt (verificeret mod samme kilde, qjsonparser.cpp 100-108):
`NoError, UnterminatedObject, MissingNameSeparator, UnterminatedArray,
MissingValueSeparator, IllegalValue, TerminationByNumber, IllegalNumber,
IllegalEscapeSequence, IllegalUTF8String, UnterminatedString, MissingObject,
DeepNesting, DocumentTooLarge, GarbageAtEnd`. **`ExtraData` og `ExpectingValue`
findes ikke i Qt** — de er Python `json`-navne. Sekundær kilde:
doc.qt.io/qt-5/qjsonparseerror.html (Qt 5.15.19), samme enum, læst 2026-10-29.

---

## Bilag B — målingen, der mangler (fordobling ved 63 MiB)

```cpp
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

int main() {
    const int n = 33000000;   // ~66 000 001 byte tekst
    QByteArray line;
    line.reserve(2 * n + 2);
    line.append('[');
    for (int i = 0; i < n; ++i) { line.append('1'); if (i + 1 < n) line.append(','); }
    line.append(']');
    qDebug() << "bytes:" << line.size();

    QJsonParseError e;
    QJsonDocument d = QJsonDocument::fromJson(line, &e);
    qDebug() << "error:" << e.error << e.errorString() << "offset:" << e.offset;
    return e.error == QJsonParseError::NoError ? 0 : 1;
}
```

```
g++ -O2 -std=c++11 -fPIC p.cpp -o p $(pkg-config --cflags --libs Qt5Core) && ./p
```

`NoError` → fordoblingen holder ved 63 MiB. `DocumentTooLarge` → loftet i §4.2
skal **ned**. Kør på målmaskinen mod samme Qt5 som CI bruger, og noter resultat +
dato + maskine her.

---

## Bilag C — interne koder (spor kun, aldrig på tråden)

| Kode | Hændelse | Række i §6.2 | Tråden |
|---|---|---|---|
| 1000 | reserveret, ubrugt | — | nej |
| 1001 | L1, linje over loft | L1 | nej |
| 2001 | P1, fortolkningsfejl | P1 | nej |
| 2002 | L2, `DocumentTooLarge` | L2 | nej |
| 2003 | L3, `DeepNesting` | L3 | nej |
| 3001 | P2/P3, ikke en meddelelse | P2, P3 | nej |
| 3002 | P4, ukendt svar-id | P4 | nej |
| 3003 | P5, ukendt metode | P5 | nej |
| 3004 | agentstien ikke nåbar (§9.2e) | N1 | nej |
| 4001 | C1, EOF med udestående | C1 | nej |
| 4002 | C2, unormal afslutning | C2 | nej |

`1002` er udgået i v3. Koden beskrev en klient-syntetisk timeout og bar
trådkoden −33000, som §0.3 og §6.1 forbyder. Den havde ingen hændelse i §6.2.
Hullet i 1-blokken må ikke genbruges: et nyt punkt i blokken kræver en hændelse,
og nogen skal beslutte, at den findes.

---

## Bilag D — kilder, metode, dato, maskine

**Hentet/læst i denne leverance (2026-10-29, målmaskine medmindre andet står):**

| Kilde | Metode | Anvendt til |
|---|---|---|
| JSON-RPC 2.0 Specification, opdateret 2013-01-04, jsonrpc.org/specification | web-hentning, hele dokumentet læst | §0.3, §2.5, §5, §6.1, §6.3, §8 (koder, id:null, batch-eksempler) |
| PyPI JSON-metadata for `agent-client-protocol` 0.11.0 + wheel `agent_client_protocol-0.11.0-py3-none-any.whl` (sha256 ikke målt; måles ved alpha, upload 2026-07-05) | web-hentning + `pip download` + udpakning | §0.1, §6.1, §9.2d; beskrivelsen »Releases track the upstream ACP schema« |
| SDK 0.11.0 læst lokalt: `acp/exceptions.py` (alle `RequestError`-hjælpere + koder), `acp/connection.py` (stdio docstring), `acp/stdio.py` (`readline`) | fil-læsning i udpakket wheel | §2.1, §6.1 (−32002 [M], −32800 ikke fundet) |
| sailjail-permissions README (master), raw.githubusercontent.com/sailfishos/sailjail-permissions/master/README.md | web-hentning, læst | §9.2e (tre skrivestier, tom-map-reglen, AppLaunch, Compatibility, Internet, én-binær-forudsætning) |
| sailfishos/qtbase mer-5.6 @ `0f76bfed2ada0ffc9a653f13c2799094e8f7dbee` | sparse clone på målmaskinen, filer læst | hele Bilag A |
| doc.qt.io/qt-5/qjsonparseerror.html (Qt 5.15.19) | web-hentning | Bilag A enum-krydstjek |
| mistralai/mistral-vibe tag `v3.0.2`, `vibe/acp/entrypoint.py` | GitHub API `contents` + base64 | §9.2a (`--setup` → onboarding → `sys.exit(0)`) |

**Målt på enheden (Jolla Phone 2026, Sailfish OS 5, aarch64), rapporteret
2026-10-26 til 2026-10-29:** agent installeret som `mistral_vibe` 3.0.2 med
`agent_client_protocol` 0.11.0 (venv under `$HOME/.local/share/skrynja/agent/`,
`pip show`), binær `vibe-acp` i `$HOME/.local/bin`, wire-format NDJSON målt runde
3-4. Se dette repos målejournaler for detaljer — men bemærk: denne spec forudsætter
ikke dem; værdierne står her med dato og maskine.

**Ikke målt:** punkt 122 (nåbarhed fra indespærret app), Bilag B (fordobling ved
63 MiB), punkt 118 (agentens faktiske fejlkoder på wire), [A110] (præcis
qtbase-version på enheden).

