use agent_client_protocol::{AcpAgent, AcpAgentConfig};
use agent_client_protocol_http::AcpHttpServer;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let addr = std::env::var("BRO_ADDR").unwrap_or_else(|_| "127.0.0.1:18923".to_string());

    let server = AcpHttpServer::new(|| {
        let mut config = AcpAgentConfig::new("/usr/local/bin/vibe-acp");
        config = config.env("PYTHONPATH", "/workspace/malelab");
        AcpAgent::new(config)
    });

    let router = server.into_router();
    let listener = tokio::net::TcpListener::bind(&addr).await?;
    eprintln!("[bro] lytter på http://{addr}/acp");
    axum_listen(listener, router).await
}

async fn axum_listen(
    listener: tokio::net::TcpListener,
    router: axum::Router,
) -> anyhow::Result<()> {
    axum::serve(listener, router).await?;
    Ok(())
}
