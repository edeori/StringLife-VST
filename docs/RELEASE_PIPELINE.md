# Moth Production VST release pipeline

## Működés

1. A mothproduction.hu Shop adminban felveszed a terméket, a GitHub repót és a két stabil letöltési URL-t. Bekapcsolod a „GitHub build pipeline engedélyezve” mezőt.
2. A repó Actions → Release StringLife → Run workflow műveletével indítasz tesztbuildet. Alapértelmezetten csak GitHub artifact készül; az éles MinIO-feltöltéshez jelöld be a `publish` mezőt. A `v*` tag push automatikusan publikál. A Shop mentése önmagában nem indít buildet.
3. A workflow hitelesítve lekéri a terméket a `/release-api/products/<id>` végpontról. Ellenőrzi a repót és a feltöltési célokat, még a build előtt.
4. GitHub-hosted macOS és Windows gépeken párhuzamosan elkészül a telepítő. A macOS csomag universal arm64/x86_64, Developer ID-val aláírt és notarizált. A Windows x64 telepítő jelenleg nincs Authenticode-dal aláírva.
5. Csak mindkét build sikere után tölti fel a telepítőket a MinIO-ba. A GitHub artifactok 30 napig megmaradnak.

StringLife stabil URL-jei:

```
https://download.mothproduction.hu/downloads/plugins/stringlife/mac/StringLife-Installer.pkg
https://download.mothproduction.hu/downloads/plugins/stringlife/win/StringLife-Setup-Win.exe
```

A verzió továbbra is a CMake projektben és a telepítők belsejében szerepel. A MinIO objektum neve nem változik. Az új kiadás felülírja az előzőt. A két objektum feltöltése nem közös tranzakció: hálózati hiba esetén részleges publikálás lehetséges, ilyenkor a publish jobot kell újrafuttatni. Ne futtass újra régi kiadást egy újabb sikeres kiadás után: az visszaírná a régi csomagokat. Visszaállíthatósághoz MinIO bucket versioning javasolt; ezt a workflow nem kapcsolja be.

## Egyszeri üzembe helyezés

A változtatások két repóban vannak: a StringLife repóban a workflow és CI scriptek, a mothproduction-web repóban a `release-registry` plugin, a Shop blueprint két új mezője és az environment továbbítása. A webes API 2026-10-19-én célzottan ki lett telepítve. A GitHub workflow telepítése külön, a release pipeline commitjával történik.

A weben a `release-registry` plugint és a módosított `themes/custom/blueprints/shop.yaml` fájlt célzottan kell kitelepíteni. Az élő Shop oldalt ne írd felül a helyi példánnyal. A szerver `.env` fájljába kerüljön egy véletlen `RELEASE_REGISTRY_TOKEN`, és a Grav service environment részébe ennek továbbítása, ahogy a helyi compose-ban látható. A szerver teljes compose fájlját ne másold felül. Az environment változás konténer-újralétrehozást igényel.

GitHub repository vagy organization secrets:

| Secret | Tartalom |
| --- | --- |
| `REGISTRY_TOKEN` | A szerver `RELEASE_REGISTRY_TOKEN` értéke |
| `APPLE_CERTIFICATES_P12` | Base64 PKCS#12 export, Developer ID Application **és** Developer ID Installer tanúsítvány, privát kulcsokkal |
| `APPLE_CERTIFICATES_PASSWORD` | Az export jelszava |
| `APPLE_ID` | Apple Developer-fiók email |
| `APPLE_APP_PASSWORD` | Notarizációhoz használt alkalmazásjelszó |
| `APPLE_TEAM_ID` | Apple team azonosító |
| `MINIO_ACCESS_KEY` | Feltöltési jogosultságú technikai felhasználó access key |
| `MINIO_SECRET_KEY` | Hozzá tartozó secret key |

Opcionális GitHub variables: `CODESIGN_IDENTITY`, `INSTALLER_IDENTITY`. Üresen a meglévő package.sh alapértelmezései érvényesek. A helyi Keychain tanúsítványait és notary profilját a GitHub gépek nem öröklik. A titkok nem kerülnek gitbe, Shop-termékbe vagy API-válaszba.

A MinIO technikai felhasználó hozzáférését a termék prefixére korlátozd. A `download.mothproduction.hu` végpontnak fogadnia kell a hitelesített S3 feltöltéseket. A `MINIO_SETUP.md` opcionális GET/HEAD-only Nginx szabályával ez nem működik: azt az adminnak célzottan módosítania kell, vagy a publish jobot belső hálózati runnerre kell átállítani. A pipeline nem módosít tűzfalat, Nginxet vagy bucket policyt. A stabil URL-eknél a CDN/proxy se szolgáljon ki korábbi cache-elt csomagot; a feltöltés `Cache-Control: no-cache` fejlécet állít.

Az első éles teszt előtt állítsd be a Shop-termék új mezőit és a fenti stabil URL-eket. Az URL-váltás és az első feltöltés között az új cím még nem működik; a terméket csak sikeres publikálás után jelöld letölthetőnek. Egy tag indításához annak a workflow-fájlokat tartalmazó commitra kell mutatnia; a kézi indítás workflow-ja legyen az alapértelmezett ágon.

## Új VST bekötése

A `vst-release.yml` `workflow_call` workflow közös repóból is hívható, például `edeori/vst-pipeline/.github/workflows/vst-release.yml@<rögzített-commit>`. Jelenleg a StringLife repóban található; nincs külön központi repó létrehozva.

Az új projekt hívó workflow-jában csak a `product-id` és a közös workflow hivatkozása változik. A checkout mindig a hívó projekt forrását tölti le. Minden hívó projektben szükséges:

- `ci/registry.py` és `ci/apple-signing.sh`: a közös segédek másolata.
- `ci/package-mac.sh`: a projekt telepítőjét állítsa elő `dist/installer.pkg` néven, hibánál nem nulla kilépéssel.
- `ci/package-windows.ps1`: ugyanaz Windowsra, `dist/installer.exe` néven.
- A szükséges secrets, valamint a Shop regisztrációja, ahol a repó és termékazonosító egyezik.

A közös workflow a végrehajtási sorrendet, aláírás előkészítését, artifactkezelést és feltöltést adja. A projektenként eltérő build és telepítő a két adapterben marad. Egy következő központosításkor a két közös segéd composite actionbe vihető, így azokat sem kell másolni.

## Ellenőrzés

Helyben: `python3 -m unittest discover -s ci -p 'test_*.py'`, `bash -n ci/*.sh`, PowerShell parser, valamint `actionlint` a workflow-kra. A tényleges platformbuild, Apple notarizáció és MinIO-feltöltés csak a secrets és a webes API élesítése után ellenőrizhető. A workflow jelenleg nem futtat DAW-host vagy pluginval DSP teszteket.

Referenciák: [GitHub reusable workflows](https://docs.github.com/en/actions/how-tos/reuse-automations/reuse-workflows), [S3 CLI feltöltés](https://docs.aws.amazon.com/cli/latest/reference/s3/cp.html).

## Éles szerver beállítása (2026-10-19)

- Szerver: `mothstack`, webes gyökér: `/srv/mothproduction-web`.
- API: `https://mothproduction.hu/release-api/products/stringlife`; Bearer-token szükséges.
- A `release-registry` plugin és a Shop blueprint ki van telepítve, a StringLife release mezői beállítva. A termék publikáltsága változatlan.
- A Grav service megkapja a `RELEASE_REGISTRY_TOKEN` környezeti változót. Újralétrehozás után várd meg az indulását, majd a cache-t `1000:1000` felhasználóval kezeld.
- A MinIO `stringlife-github` felhasználó `stringlife-release` policyja csak a `downloads/plugins/stringlife/*` objektumokra enged feltöltést és olvasást. Más termékre írás tesztelve: AccessDenied.
- A külső HTTPS S3-feltöltés és publikus visszaolvasás sikeres volt; a tesztobjektum törölve. Az NPM módosítására nem volt szükség.
- A meglévő `StringLife.pkg` másolata az új `StringLife-Installer.pkg` címen is elérhető. A Windows URL csak az első sikeres publikálás után működik.
- A szerver eredeti `.env`, compose, Shop oldal és blueprint mentése: `.deploy-backups/release-pipeline-20261019/` (csak root hozzáféréssel).
- A tanúsítványok, registry-token és korlátozott MinIO-hitelesítők GitHub Secretsben vannak. A notarizációhoz az `APPLE_APP_PASSWORD` is szükséges; a helyi `moth-notary` profilt a runner nem éri el.
