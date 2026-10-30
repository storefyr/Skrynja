use agent_client_protocol::Client;
use agent_client_protocol::schema::{ProtocolVersion, v1::InitializeRequest};
use agent_client_protocol_http::HttpClient;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let base = std::env::var("BRO_URL").unwrap_or_else(|_| "http://127.0.0.1:18923/acp".to_string());
    eprintln!("[klient] forbinder til {base}");

    let transport = HttpClient::new(&base)?;

    Client::default()
        .builder()
        .name("skrynja-malelab")
        .connect_with(transport, async |cx| {
            let response = cx
                .send_request(InitializeRequest::new(ProtocolVersion::V1))
                .block_task()
                .await?;
            eprintln!("[klient] initialize OK:");
            eprintln!("[klient]   protocol_version = {:?}", response.protocol_version);
            eprintln!("[klient]   agent_info = {:?}", response.agent_info);
            eprintln!("[klient]   agent_capabilities = {:?}", response.agent_capabilities);
            Ok(())
        })
        .await?;

    eprintln!("[klient] roundtrip gennem bro fuldført");
    Ok(())
}
