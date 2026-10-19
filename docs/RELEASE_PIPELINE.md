# StringLife release pipeline

A build a **StringLife-VST saját Actions felületén** fut. A helyi `.github/workflows/release.yml` a publikus [edeori/vst-release-pipeline](https://github.com/edeori/vst-release-pipeline) repó `.github/workflows/vst-release.yml` workflow-ját hívja rögzített commit SHA-val. Így publikus és privát VST-repók egyaránt használhatják ugyanazt a pipeline-t.

## Teszt és kiadás

**StringLife-VST → Actions → Release installers → Run workflow**:

- `publish=false` (alapértelmezés): tesztbuild, telepítők a StringLife Actions artifactjai között.
- `publish=true`: mindkét sikeres build után kézi jóváhagyásra vár a `production` environmentben, és csak utána tölt fel a MinIO-ba.
- `v*` tag push: automatikus build a tag commitjából; az éles publikálás itt is kézi jóváhagyást igényel.

```sh
gh workflow run release.yml --repo edeori/StringLife-VST --ref main -f publish=false
```

A workflow az esemény `github.sha` commitjából építi mindkét platformot. A helyi, nem commitolt módosítások nem kerülnek bele. Az artifactok 30 napig elérhetők.

## Projektspecifikus részek és titkok

- `ci/package-mac.sh`: aláírt, notarizált universal macOS PKG → `dist/installer.pkg`.
- `ci/package-windows.ps1`: Windows x64 Inno Setup telepítő → `dist/installer.exe`.
- A `package.sh` átveszi a központilag beállított `NOTARY_PROFILE=ci-notary` értéket.
- A telepítőverzió a CMake projektverzióból származik.

A registry-validáció, az Apple-aláírás előkészítése és a publikálási sorrend a közös workflow-ban van. A titkok a **StringLife repository Secrets** beállításaiban vannak: `REGISTRY_TOKEN`, `APPLE_CERTIFICATES_P12`, `APPLE_CERTIFICATES_PASSWORD`, `APPLE_ID`, `APPLE_APP_PASSWORD`, `APPLE_TEAM_ID`, `MINIO_ACCESS_KEY`, `MINIO_SECRET_KEY`. A hívó explicit átadja őket. A közös publikus repó nem tárol kiadási titkot.

Új VST bekötését és a közös workflow verziófrissítését a közös repó README-je írja le. A helyi Apple-beállítás a login Kulcskarikában `moth-vst-release-ci` néven van; a `moth-notary` profil változatlan marad.

## Web és tárhely

A Shop `stringlife` termékében a pipeline engedélyezve, a forrásrepó `edeori/StringLife-VST`. A tokennel védett konfigurációs végpont:

`https://mothproduction.hu/release-api/products/stringlife`

Állandó letöltési címek:

- https://download.mothproduction.hu/downloads/plugins/stringlife/mac/StringLife-Installer.pkg
- https://download.mothproduction.hu/downloads/plugins/stringlife/win/StringLife-Setup-Win.exe

A macOS URL-en a korábbi telepítő másolata már elérhető; a Windows URL az első éles publikálás után működik. A Windows telepítő jelenleg nincs Authenticode-dal aláírva. A két MinIO-objektum feltöltése nem atomi; részleges feltöltési hiba esetén a publish job újrafuttatható. Régi kiadás újrafuttatása felülírhat frissebb telepítőt.

A szerver: `mothstack`, webgyökér: `/srv/mothproduction-web`. A 2026-10-19-i API-telepítés előtti fájlmentések: `.deploy-backups/release-pipeline-20261019/`. A Grav konténer továbbítja a `RELEASE_REGISTRY_TOKEN` környezeti változót. Cache-kezelés a konténer indulása után `1000:1000` felhasználóval történjen.

Ellenőrizve: tokenes API 200; hibás token 401; POST 405; ismeretlen termék 404; külső HTTPS MinIO-feltöltés és publikus visszaolvasás; más termék prefixére írás tiltása. A korábbi központi tesztben a Windows EXE és aláírt, notarizált macOS PKG is elkészült; az új reusable workflow tesztje már a StringLife Actions felületén látható.

2026-10-19: a [StringLife saját reusable workflow-tesztje](https://github.com/edeori/StringLife-VST/actions/runs/39251229057) sikeres. A Windows EXE és az aláírt, notarizált macOS PKG a StringLife artifactjai között elérhető; a korábbi Node.js 20-as figyelmeztetés megszűnt (csak macOS runner-kapacitásról szóló tájékoztató notice maradt). Forráscommit: `b67a51a` (1.6.2). A teszt nem publikált a MinIO-ba.

A kijelölt jóváhagyó `edeori`. A sikeres build után: **Review deployments → production → Approve and deploy**. Saját indítás jóváhagyható; admin bypass nincs engedélyezve. A `publish=false` teszt nem kér jóváhagyást és nem tölt fel.

A jóváhagyási kapu tesztje: [39252600197](https://github.com/edeori/StringLife-VST/actions/runs/39252600197). A job `waiting` állapotban állt meg, a pending deployment reviewer `edeori` volt. A próba feltöltést nem végzett, ellenőrzés után megszakítottuk.
