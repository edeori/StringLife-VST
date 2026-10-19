# StringLife release pipeline

A build most a privát [edeori/vst-release-pipeline](https://github.com/edeori/vst-release-pipeline) repóban fut. A StringLife forrása ebben a repóban marad. A központi pipeline és az új VST-k bekötésének leírása a központi repó README-jében található.

## Teszt vagy kiadás indítása

A központi repó Actions → **Build VST installers** → Run workflow felületén:

- `product`: `stringlife`
- `ref`: forráság, tag vagy commit SHA
- `publish`: kikapcsolva tesztbuild, bekapcsolva éles MinIO-feltöltés

```sh
gh workflow run release.yml --repo edeori/vst-release-pipeline --ref main \
  -f product=stringlife -f ref=main -f publish=false
```

A központi workflow rögzíti a forráscommitot, majd abból építi mindkét platformot. A helyi, nem commitolt pluginmódosítások nem kerülnek bele. A telepítők 30 napig GitHub artifactként elérhetők. MinIO-feltöltés csak mindkét build sikere és `publish=true` esetén történik.

A StringLife `v*` tag push önmagában már nem indít buildet. A tag nevét a központi indítás `ref` mezőjében add meg. Automatikus repo-közi indításhoz külön hitelesített trigger szükséges.

## Projektspecifikus részek

- `ci/package-mac.sh`: aláírt, notarizált universal macOS PKG → `dist/installer.pkg`.
- `ci/package-windows.ps1`: Windows x64 Inno Setup telepítő → `dist/installer.exe`.
- A `package.sh` átveszi a központilag beállított `NOTARY_PROFILE=ci-notary` értéket.
- A verzió a CMake projektverzióból származik.

A közös registry-validáció és Apple-aláírás előkészítése a központi repóban található. Apple- és MinIO-hitelesítők nem szükségesek a StringLife repóban. A központi `stringlife` environment külön forrásolvasási SSH deploy keyt és csak a StringLife MinIO-prefixére jogosult feltöltőkulcsot használ.

## Web és tárhely

A Shop `stringlife` termékében a pipeline engedélyezve, a forrásrepó `edeori/StringLife-VST`. A tokennel védett konfigurációs végpont:

`https://mothproduction.hu/release-api/products/stringlife`

Állandó letöltési címek:

- https://download.mothproduction.hu/downloads/plugins/stringlife/mac/StringLife-Installer.pkg
- https://download.mothproduction.hu/downloads/plugins/stringlife/win/StringLife-Setup-Win.exe

A macOS URL-en a korábbi telepítő másolata már elérhető; a Windows URL az első éles publikálás után működik. A Windows telepítő jelenleg nincs Authenticode-dal aláírva. A két MinIO-objektum feltöltése nem atomi; részleges feltöltési hiba esetén a publish job újrafuttatható. Régi kiadás újrafuttatása felülírhat frissebb telepítőt.

A szerver: `mothstack`, webgyökér: `/srv/mothproduction-web`. A 2026-10-19-i API-telepítés előtti fájlmentések: `.deploy-backups/release-pipeline-20261019/`. A Grav konténer továbbítja a `RELEASE_REGISTRY_TOKEN` környezeti változót. Cache-kezelés a konténer indulása után `1000:1000` felhasználóval történjen.

Ellenőrizve: tokenes API 200; hibás token 401; POST 405; ismeretlen termék 404; külső HTTPS MinIO-feltöltés és publikus visszaolvasás; más termék prefixére írás tiltása. A központi teljes platformbuild eredménye az Actions-futásban követhető.
