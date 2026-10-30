try:
    import mcp.client.streamable_http as _sh
    if not hasattr(_sh, "streamablehttp_client") and hasattr(_sh, "streamable_http_client"):
        _sh.streamablehttp_client = _sh.streamable_http_client
except Exception:
    pass

try:
    import mcp.shared.context as _ctx
    if not hasattr(_ctx, "RequestContext"):
        from typing import Any, Generic, TypeVar

        _SessionT = TypeVar("_SessionT")
        _ReqT = TypeVar("_ReqT")

        class RequestContext(Generic[_SessionT, _ReqT]):
            pass

        _ctx.RequestContext = RequestContext
except Exception:
    pass
