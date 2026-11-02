# Målejournal runde 7: a–g målt på telefonen 2026-11-02 (grid-start, inde fra jailen)

**Maskine:** Jolla Phone 2026, Sailfish OS 5, aarch64. **Dato:** 2026-11-02. **Byg:** PR #19 @ `ce8a99f` (in-jail diag i `harbour-skrynja.cpp`), installeret via `sh test-pr.sh 19`, startet fra grid; rå log i `/tmp/skrynja-app.log`. Formål: målingerne a–g som forudsætning for ny afgørelse om §9.2e (punkt 131, [A] indtil videre).

## Rå output

App-diag (grid-start, inde fra jailen), verbatim:

    [acp][diag] uid 100000 home "/home/defaultuser" cwd "/home/defaultuser" exists false isSymLink false canonical "" dotlocal_exists true bin_list () home_list ("android_storage", "Desktop", "Documents", "Downloads", "Music", "Pictures", "Playlists", "Public", "Templates", "Videos")
    [acp][diag] open_test false "No such file or directory"
    [acp][diag] exec_from_mounted true 0 "jail-exec-ok" "Unknown error"
    [acp][diag] net_1.1.1.1:443 true QAbstractSocket::ConnectedState "Unknown error"
    [acp][diag] projectroot false 0
    [acp][diag] mounted_writable true "Unknown error"

mountinfo for app-processen (pid 15257), uddrag (devel-su, uden for jailen):

    4210 ... 0:149 / /home rw,nosuid,nodev - tmpfs ... (tmpfs over /home)
    4211 ... /defaultuser /home/defaultuser rw,noatime - ext4 /dev/mapper/luks-...
    4246 ... 0:156 / /home rw,nosuid,nodev,noexec - tmpfs ...
    4250 ... /defaultuser/.local/share/harbour-skrynja /home/defaultuser/.local/share/harbour-skrynja rw,noatime - ext4 ...
    (tilsvarende bind-mounts for Documents, Pictures, Videos, Desktop, .config/*, .cache/*, .local/share/applications, .local/nemo-transferengine)

Terminal (uden jail), samme dag:

    $ head -2 /home/defaultuser/.local/share/uv/tools/mistral-vibe/bin/vibe-acp
    #!/home/defaultuser/.local/share/uv/tools/mistral-vibe/bin/python
    # -*- coding: utf-8 -*-
    $ ls -ld /home/defaultuser/.local/share/harbour-skrynja
    drwx------ 3 defaultuser defaultuser 4096 Oct 20 14:32 ...
    $ df -h /home | tail -1
    218.0G  15.9G  191.0G  8% /home

## Målt a–g

| # | Spørgsmål | Resultat | [M] |
|---|---|---|---|
| a | Kan en proces køres fra den monterede mappe (noexec?) | **Ja** — busybox kopieret til `.local/share/harbour-skrynja/bin/` blev exec'et af appen inde fra jailen: exit 0, stdout "jail-exec-ok". Bind-mountet er `rw,noatime` (ikke noexec). | 2026-11-02 |
| b | Netværk inde fra jailen | **Ja** — TCP connect til 1.1.1.1:443 fra app-processen: ConnectedState. | 2026-11-02 |
| c | Er projektets rodmappe synlig? (§9.2 felt 3) | **Nej** — `/home/defaultuser/Skrynja` findes ikke i jailen (exists false, 0 entries). Kun XDG-standardmapperne + ovenstående bind-mounts er synlige. | 2026-11-02 |
| d | Kan både binær og venv ligge i den monterede mappe? | **Ja, teknisk** — `.local/share/harbour-skrynja` er skrivbar (write-test OK) og der er 191G ledig på /home. Men: shimmen peger via shebang på `$HOME/.local/share/uv/tools/mistral-vibe/bin/python`, som ikke er monteret — venv OG shim skal begge ligge i en monteret mappe. | 2026-11-02 |
| e | Er uv-tool-shimmen ét led eller to? (§9.2c) | **Ét led** — shimmen er et python-script hvis shebang går direkte til venv'ets python (`#!/home/defaultuser/.local/share/uv/tools/mistral-vibe/bin/python`); kernen starter fortolkeren direkte, intet ekstra procesled. Forudsætter at shebang-stien er nåbar (se d). | 2026-11-02 |
| f | Agentens versionsstreng ved initialize | **3.0.2** — `agentInfo.version` i initialize-svaret, målt runde 5 og 6 (`maalejournal-runde5/6`), `protocolVersion: 1` begge veje. | 2026-11-02 |
| g | Hvornår kommer MISTRAL_API_KEY ind? | **Ikke målt** — miljøvariabel, ikke filsystem. Formodning (ikke måling): arves fra app-miljøet; session/new og et simpelt prompt virkede i REPL uden synlig nøgle i skal-miljøet (runde 6), så nøglen først bliver nødvendig ved rigtige modelkald. Skal måles ved første rigtige LLM-svar. | — |

## R87 tillæg (samme dag, terminal): startkæden målt via ps

    3423 /usr/libexec/mapplauncherd/booster-silica-session --boot-mode
    3497 /usr/libexec/mapplauncherd/booster-qt5 --systemd --boot-mode
    3498 /usr/libexec/mapplauncherd/booster-generic --systemd
    3745 /usr/bin/invoker --type=silica-session -- /usr/bin/sailjail --profile=jolla-camera -- /usr/libexec/mapplauncherd/booster-silica-media --application=jolla-camera
    3490 /usr/bin/firejail --quiet --template=OrganizationName:com.jolla --template=ApplicationName:camera --private-bin=... --whitelist=...

Målt: `invoker` kører `/usr/bin/sailjail` for silica-apps, og sailjail/firejail-argumenterne bygges fra `--template=OrganizationName:<org> --template=ApplicationName:<app>` — dvs. også UDEN `[X-Sailjail]`-sektion i .desktop kører grid-start gennem sailjail. Kilde til whitelist-listen (hvilke $HOME-mapper der bind-mountes) er endnu ikke målt; mapplauncherd/booster er kandidaten. [M] 2026-11-02.

## Konsekvens (ikke beslutning)

Grid-start kører i firejail; `.local/bin` og projektroden er ikke nåbare derfra; `.local/share/harbour-skrynja` er nåbar, skrivbar og exec-tilladt, og netværk virker. Programsti (§9.2 felt 1) kan derfor målt pege ind i en monteret mappe, hvis både shim og venv lægges der. Dette kræver ny [B] fra storefyr med ny dato før §9.2e omskrives — ellers står to [B] med modsat indhold. Indtil da: §12 punkt 131 [A].
