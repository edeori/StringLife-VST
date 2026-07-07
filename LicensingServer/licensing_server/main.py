from __future__ import annotations

import ipaddress

from fastapi import Depends, FastAPI, Form, HTTPException, Query
from fastapi.requests import Request
from fastapi.responses import HTMLResponse, PlainTextResponse, RedirectResponse
from sqlmodel import Session

from .config import settings
from .database import check_database_connection, create_db_and_tables, get_session
from .schemas import AdminOverviewResponse, CreateLicenseRequest, CreateProductRequest, LicenseRequest, LicenseResponse
from .service import (
    activate_license,
    create_license_record,
    create_product_record,
    deactivate_license,
    delete_license_record,
    delete_product_record,
    get_admin_overview,
    refresh_license,
    set_license_comment,
    set_license_max_activations,
    toggle_license_enabled,
    validate_license,
)


app = FastAPI(title="Plugin Licensing Server", version="0.1.0")

ADMIN_PATH_PREFIXES = ("/admin",)
PUBLIC_API_PATHS = {"/activate", "/validate", "/refresh", "/deactivate"}
_admin_allowed_networks = tuple(ipaddress.ip_network(value, strict=False) for value in settings.admin_allowed_cidrs)
_trusted_proxy_networks = tuple(ipaddress.ip_network(value, strict=False) for value in settings.trusted_proxy_cidrs)


def _ip_in_networks(ip_text: str, networks: tuple[ipaddress._BaseNetwork, ...]) -> bool:
    try:
        ip_value = ipaddress.ip_address(ip_text)
    except ValueError:
        return False

    return any(ip_value in network for network in networks)


def _resolve_client_ip(request: Request) -> str:
    direct_peer = request.client.host if request.client else ""

    if not settings.trust_proxy_headers or not direct_peer:
        return direct_peer

    if not _ip_in_networks(direct_peer, _trusted_proxy_networks):
        return direct_peer

    forwarded_for = request.headers.get("x-forwarded-for", "")

    if not forwarded_for:
        return direct_peer

    first_hop = forwarded_for.split(",", 1)[0].strip()
    return first_hop or direct_peer


@app.middleware("http")
async def restrict_admin_to_allowed_networks(request: Request, call_next):
    path = request.url.path

    if path.startswith(ADMIN_PATH_PREFIXES) and not settings.expose_admin_ui:
        return PlainTextResponse("Admin UI is disabled on this listener", status_code=404)

    if path in PUBLIC_API_PATHS and not settings.expose_public_api:
        return PlainTextResponse("Public licensing API is disabled on this listener", status_code=404)

    if path.startswith(ADMIN_PATH_PREFIXES):
        client_ip = _resolve_client_ip(request)

        if not _ip_in_networks(client_ip, _admin_allowed_networks):
            return HTMLResponse("Admin access is restricted", status_code=403)

    return await call_next(request)


def render_admin_page(data: AdminOverviewResponse) -> str:
    create_product_options = "\n".join(
        f'<option value="{product_id}">{product_id}</option>'
        for product_id in data.availableProductIds
    )
    product_options = "\n".join(
        f'<option value="{product_id}"{" selected" if product_id == data.selectedProductId else ""}>{product_id}</option>'
        for product_id in data.availableProductIds
    )

    product_badge = data.selectedProductId if data.selectedProductId else "All products"
    product_rows = "\n".join(
        f"""
        <tr>
          <td><strong>{product.displayName}</strong><br><code>{product.productId}</code></td>
          <td>{product.totalLicenses}</td>
          <td>{product.activeActivations}</td>
          <td>{'yes' if product.isEnabled else 'no'}</td>
          <td>{product.createdAt}</td>
          <td>
            <form method="post" action="/admin/products/delete" onsubmit="return confirm('Delete product {product.productId}? This will also delete all licenses and activations for this plugin.');" style="display:flex; gap:8px; margin:0;">
              <input type="hidden" name="product_id" value="{product.productId}">
              <button type="submit">Delete</button>
            </form>
          </td>
        </tr>
        """
        for product in data.products
    )
    license_rows = "\n".join(
        f"""
        <tr>
          <td><code>{license.licenseKey}</code></td>
          <td>{license.productId}</td>
          <td>{license.maxActivations}</td>
          <td>{license.activeActivationCount}</td>
          <td>{'yes' if license.isEnabled else 'no'}</td>
          <td>{'yes' if license.isRevoked else 'no'}</td>
          <td>{license.comment or ''}</td>
          <td>{license.createdAt}</td>
          <td>
            <div style="display:flex; gap:8px; align-items:center; flex-wrap:nowrap;">
            <form method="post" action="/admin/licenses/set-max-activations" style="display:flex; gap:8px; align-items:center; margin:0;">
              <input type="hidden" name="license_key" value="{license.licenseKey}">
              <input type="hidden" name="product_id" value="{license.productId}">
              <input type="number" min="{max(1, license.activeActivationCount)}" name="max_activations" value="{license.maxActivations}" style="width:88px;">
              <button type="submit">Save</button>
            </form>
            <form method="post" action="/admin/licenses/toggle" style="display:flex; gap:8px; margin:0;">
              <input type="hidden" name="license_key" value="{license.licenseKey}">
              <input type="hidden" name="product_id" value="{license.productId}">
              <button type="submit">{"Disable" if license.isEnabled else "Enable"}</button>
            </form>
            <form method="post" action="/admin/licenses/delete" onsubmit="return confirm('Delete license {license.licenseKey}? This will also delete every activation attached to it.');" style="display:flex; gap:8px; margin:0;">
              <input type="hidden" name="license_key" value="{license.licenseKey}">
              <input type="hidden" name="product_id" value="{license.productId}">
              <button type="submit">Delete</button>
            </form>
            </div>
            <form method="post" action="/admin/licenses/set-comment" style="display:flex; gap:8px; align-items:center; margin:8px 0 0;">
              <input type="hidden" name="license_key" value="{license.licenseKey}">
              <input type="hidden" name="product_id" value="{license.productId}">
              <input type="text" name="comment" value="{license.comment}" placeholder="Internal note: customer, order, invoice..." style="min-width:280px;">
              <button type="submit">Note</button>
            </form>
          </td>
        </tr>
        """
        for license in data.licenses
    )

    activation_rows = "\n".join(
        f"""
        <tr>
          <td><code>{activation.activationId}</code></td>
          <td><code>{activation.licenseKey}</code></td>
          <td>{activation.productId}</td>
          <td><code>{activation.machineId}</code></td>
          <td>{activation.platform}</td>
          <td>{activation.pluginVersion}</td>
          <td>{activation.activatedAt}</td>
          <td>{activation.lastValidatedAt}</td>
          <td>{activation.deactivatedAt or ''}</td>
        </tr>
        """
        for activation in data.activations
    )

    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Plugin Licensing Admin</title>
  <style>
    :root {{
      --bg: #0b1114;
      --panel: #121b1f;
      --panel-2: #182329;
      --line: #2d434c;
      --text: #edf6f7;
      --muted: #8ea8b2;
      --accent: #51d7ff;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      font-family: ui-sans-serif, -apple-system, BlinkMacSystemFont, sans-serif;
      background: radial-gradient(circle at top, #16303a 0%, var(--bg) 45%);
      color: var(--text);
    }}
    main {{ max-width: 1200px; margin: 0 auto; padding: 32px 20px 56px; }}
    h1, h2 {{ margin: 0 0 12px; }}
    p {{ color: var(--muted); margin: 0; }}
    .grid {{ display: grid; grid-template-columns: 320px 1fr; gap: 20px; margin-top: 24px; }}
    .toolbar {{ display: flex; gap: 12px; align-items: end; margin-top: 20px; flex-wrap: wrap; }}
    .panel {{
      background: rgba(18, 27, 31, 0.94);
      border: 1px solid var(--line);
      border-radius: 18px;
      padding: 18px;
      box-shadow: 0 14px 40px rgba(0, 0, 0, 0.28);
    }}
    .stats {{ display: flex; gap: 12px; margin-top: 16px; }}
    .stat {{ flex: 1; background: var(--panel-2); border-radius: 14px; padding: 12px; }}
    .stat strong {{ display: block; font-size: 28px; color: var(--accent); }}
    form {{ display: grid; gap: 10px; margin-top: 16px; }}
    input, select {{
      width: 100%;
      background: #0d1417;
      color: var(--text);
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 10px 12px;
    }}
    button {{
      background: var(--accent);
      color: #041015;
      border: 0;
      border-radius: 12px;
      padding: 11px 14px;
      font-weight: 700;
      cursor: pointer;
    }}
    table {{ width: 100%; border-collapse: collapse; font-size: 14px; }}
    th, td {{ text-align: left; padding: 10px 8px; border-bottom: 1px solid rgba(45, 67, 76, 0.65); vertical-align: top; }}
    th {{ color: var(--muted); font-weight: 600; }}
    code {{ color: var(--accent); font-family: ui-monospace, SFMono-Regular, monospace; }}
    .stack {{ display: grid; gap: 20px; }}
    .eyebrow {{ color: var(--accent); font-size: 12px; text-transform: uppercase; letter-spacing: 0.12em; }}
    .filter-form {{ display: flex; gap: 10px; align-items: end; flex-wrap: wrap; margin-top: 16px; }}
    .filter-form > div {{ min-width: 240px; }}
    .subtle {{ color: var(--muted); font-size: 13px; }}
    .warning {{ color: #ffb4a2; }}
    @media (max-width: 900px) {{ .grid {{ grid-template-columns: 1fr; }} }}
  </style>
</head>
<body>
  <main>
    <div class="eyebrow">Central Licensing Tool</div>
    <h1>Plugin Licensing Admin</h1>
    <p>POC view for local testing across multiple plugins. It shows registered products, issued license keys, and plugin instances by machine hash.</p>
    <form class="filter-form" method="get" action="/admin">
      <div>
        <label class="subtle" for="product_id">Selected product</label>
        <select id="product_id" name="product_id">
          <option value="">All products</option>
          {product_options}
        </select>
      </div>
      <button type="submit">Filter</button>
    </form>
    <div class="stats">
      <div class="stat"><span>Product scope</span><strong>{product_badge}</strong></div>
      <div class="stat"><span>Licenses</span><strong>{len(data.licenses)}</strong></div>
      <div class="stat"><span>Activations</span><strong>{len(data.activations)}</strong></div>
    </div>
    <div class="grid">
      <section class="panel">
        <h2>Register product</h2>
        <p>Add a new plugin/product to the central licensing tool before issuing licenses for it.</p>
        <form method="post" action="/admin/products/create">
          <input name="display_name" value="" placeholder="Display name, for example: StringLife">
          <input name="product_id" value="" placeholder="Stable product ID, for example: stringlife">
          <button type="submit">Register product</button>
        </form>
        <p class="subtle warning">Deleting a product will cascade-delete its licenses and activations after confirmation.</p>
      </section>
      <section class="panel">
        <h2>Create license</h2>
        <p>Issue a local test license for the selected plugin/product. Leave the key blank to auto-generate a UUID-like license.</p>
        <form method="post" action="/admin/licenses/create">
          <input name="license_key" value="" placeholder="Leave blank to auto-generate, or paste a custom key">
          <select name="product_id">
            {product_options}
          </select>
          <input name="max_activations" value="2" placeholder="How many machines may activate this license">
          <button type="submit">Create license</button>
        </form>
      </section>
      <section class="stack">
        <section class="panel">
          <h2>Products</h2>
          <table>
            <thead><tr><th>Plugin</th><th>Licenses</th><th>Active activations</th><th>Enabled</th><th>Created</th><th>Actions</th></tr></thead>
            <tbody>{product_rows}</tbody>
          </table>
        </section>
        <section class="panel">
          <h2>Licenses</h2>
          <table>
            <thead><tr><th>Key</th><th>Product</th><th>Max</th><th>Active</th><th>Enabled</th><th>Revoked</th><th>Comment</th><th>Created</th><th>Actions</th></tr></thead>
            <tbody>{license_rows}</tbody>
          </table>
        </section>
        <section class="panel">
          <h2>Registered instances</h2>
          <table>
            <thead><tr><th>Activation</th><th>License</th><th>Product</th><th>Machine</th><th>Platform</th><th>Version</th><th>Activated</th><th>Last seen</th><th>Deactivated</th></tr></thead>
            <tbody>{activation_rows}</tbody>
          </table>
        </section>
      </section>
    </div>
  </main>
</body>
</html>"""


@app.on_event("startup")
def on_startup() -> None:
    create_db_and_tables()


@app.get("/health")
def health() -> dict[str, str]:
    try:
        check_database_connection()
    except Exception as exc:  # pragma: no cover - operational path
        raise HTTPException(status_code=503, detail=f"database unavailable: {exc}") from exc

    return {"status": "ok"}


@app.get("/admin", response_class=HTMLResponse)
def admin(product_id: str = Query(default=""), session: Session = Depends(get_session)) -> HTMLResponse:
    return HTMLResponse(render_admin_page(get_admin_overview(session, product_id.strip())))


@app.get("/admin/api/overview", response_model=AdminOverviewResponse)
def admin_overview(product_id: str = Query(default=""), session: Session = Depends(get_session)) -> AdminOverviewResponse:
    return get_admin_overview(session, product_id.strip())


@app.post("/admin/licenses/create")
def admin_create_license(
    license_key: str = Form(""),
    product_id: str = Form(...),
    max_activations: int = Form(2),
    session: Session = Depends(get_session),
) -> RedirectResponse:
    product_id = product_id.strip()

    create_license_record(
        session,
        CreateLicenseRequest(
            licenseKey=license_key.strip(),
            productId=product_id,
            maxActivations=max_activations,
        ),
    )
    redirect_target = "/admin" if not product_id else f"/admin?product_id={product_id}"
    return RedirectResponse(url=redirect_target, status_code=303)


@app.post("/admin/products/create")
def admin_create_product(
    display_name: str = Form(...),
    product_id: str = Form(...),
    session: Session = Depends(get_session),
) -> RedirectResponse:
    create_product_record(session, CreateProductRequest(productId=product_id, displayName=display_name))
    return RedirectResponse(url=f"/admin?product_id={product_id.strip()}", status_code=303)


@app.post("/admin/products/delete")
def admin_delete_product(
    product_id: str = Form(...),
    session: Session = Depends(get_session),
) -> RedirectResponse:
    delete_product_record(session, product_id.strip())
    return RedirectResponse(url="/admin", status_code=303)


@app.post("/admin/licenses/set-max-activations")
def admin_set_license_max_activations(
    license_key: str = Form(...),
    product_id: str = Form(...),
    max_activations: int = Form(...),
    session: Session = Depends(get_session),
) -> RedirectResponse:
    set_license_max_activations(session, license_key.strip(), product_id.strip(), max_activations)
    return RedirectResponse(url=f"/admin?product_id={product_id.strip()}", status_code=303)


@app.post("/admin/licenses/toggle")
def admin_toggle_license(
    license_key: str = Form(...),
    product_id: str = Form(...),
    session: Session = Depends(get_session),
) -> RedirectResponse:
    toggle_license_enabled(session, license_key.strip(), product_id.strip())
    return RedirectResponse(url=f"/admin?product_id={product_id.strip()}", status_code=303)


@app.post("/admin/licenses/delete")
def admin_delete_license(
    license_key: str = Form(...),
    product_id: str = Form(...),
    session: Session = Depends(get_session),
) -> RedirectResponse:
    delete_license_record(session, license_key.strip(), product_id.strip())
    return RedirectResponse(url=f"/admin?product_id={product_id.strip()}", status_code=303)


@app.post("/admin/licenses/set-comment")
def admin_set_license_comment(
    license_key: str = Form(...),
    product_id: str = Form(...),
    comment: str = Form(""),
    session: Session = Depends(get_session),
) -> RedirectResponse:
    set_license_comment(session, license_key.strip(), product_id.strip(), comment)
    return RedirectResponse(url=f"/admin?product_id={product_id.strip()}", status_code=303)


@app.post("/activate", response_model=LicenseResponse)
def activate(request: LicenseRequest, session: Session = Depends(get_session)) -> LicenseResponse:
    return activate_license(session, request)


@app.post("/validate", response_model=LicenseResponse)
def validate(request: LicenseRequest, session: Session = Depends(get_session)) -> LicenseResponse:
    return validate_license(session, request)


@app.post("/refresh", response_model=LicenseResponse)
def refresh(request: LicenseRequest, session: Session = Depends(get_session)) -> LicenseResponse:
    return refresh_license(session, request)


@app.post("/deactivate", response_model=LicenseResponse)
def deactivate(request: LicenseRequest, session: Session = Depends(get_session)) -> LicenseResponse:
    return deactivate_license(session, request)
