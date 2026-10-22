#include "settingsmanager.h"
#include "providers.h"
#include "securestore.h"
#include "conversationmanager.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QDebug>

// Injected by qmake from the spec file version
#ifndef APP_VERSION
#define APP_VERSION "2.3.0"
#endif

namespace {

struct ModelPrice {
    const char *match;      // matched as a substring of the model id
    double inputPerMillion;
    double outputPerMillion;
};

// Public list prices in USD per million tokens. Ordered from the most
// specific match to the least so "pixtral-large" does not resolve as "pixtral".
const ModelPrice PRICES[] = {
    { "pixtral-large",     2.00, 6.00 },
    { "mistral-large-4",   1.36, 4.18 },
    { "mistral-large",     0.50, 1.50 },
    { "magistral-medium",  2.00, 5.00 },
    { "mistral-medium",    1.50, 7.50 },
    { "magistral-small",   0.50, 1.50 },
    { "codestral",         0.30, 0.90 },
    { "devstral",          0.10, 0.30 },
    { "mistral-small",     0.15, 0.60 },
    { "ministral-3-14b",   0.20, 0.20 },
    { "ministral-3-8b",    0.15, 0.15 },
    { "ministral-3-3b",    0.10, 0.10 },
    { "ministral-8b",      0.10, 0.10 },
    { "ministral-3b",      0.04, 0.04 },
    { "pixtral",           0.15, 0.15 },
    { "nemo",              0.15, 0.15 },
    { "mistral-saba",      0.20, 0.60 },
    { "zai-glm",           1.40, 4.40 }
};

const int PRICE_COUNT = int(sizeof(PRICES) / sizeof(PRICES[0]));
const int MAX_SAVED_PROMPTS = 100;

}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(skrynjaSettingsPath("Skrynja"), QSettings::NativeFormat)
    , m_temperature(-1.0)
    , m_maxTokens(0)
    , m_chatStyle("flat")
    , m_autoTitleMode("local")
    , m_showTimestamps(true)
    , m_enterKeySends(false)
    , m_voiceAnonymization(false)
    , m_voicePitchFactor(1.15)
    , m_webSearch(false)
    , m_modelSwitches(0)
{
    loadSettings();
    secureSettingsFile();
}

QString SettingsManager::apiKey() const
{
    return m_apiKey;
}

void SettingsManager::setApiKey(const QString &key)
{
    if (m_apiKey != key) {
        bool hadKey = !m_apiKey.isEmpty();
        m_apiKey = key;
        bool hasKey = !m_apiKey.isEmpty();
        saveSettings();
        emit apiKeyChanged();
        if (hadKey != hasKey) {
            emit hasApiKeyChanged();
        }
    }
}

QString SettingsManager::modelName() const
{
    return m_modelName;
}

void SettingsManager::setModelName(const QString &model)
{
    if (m_modelName != model) {
        m_modelName = model;
        m_modelSwitches++;
        m_settings.setValue("stats/modelSwitches", m_modelSwitches);
        saveSettings();
        emit modelNameChanged();
    }
}

int SettingsManager::modelSwitches() const
{
    return m_modelSwitches;
}

QString SettingsManager::language() const
{
    return m_language;
}

void SettingsManager::setLanguage(const QString &lang)
{
    if (m_language != lang) {
        m_language = lang;
        saveSettings();
        emit languageChanged();
    }
}

double SettingsManager::temperature() const
{
    return m_temperature;
}

void SettingsManager::setTemperature(double temperature)
{
    if (m_temperature != temperature) {
        m_temperature = temperature;
        saveSettings();
        emit temperatureChanged();
    }
}

int SettingsManager::maxTokens() const
{
    return m_maxTokens;
}

void SettingsManager::setMaxTokens(int maxTokens)
{
    if (m_maxTokens != maxTokens) {
        m_maxTokens = maxTokens;
        saveSettings();
        emit maxTokensChanged();
    }
}

QString SettingsManager::systemPrompt() const
{
    return m_systemPrompt;
}

void SettingsManager::setSystemPrompt(const QString &prompt)
{
    if (m_systemPrompt != prompt) {
        m_systemPrompt = prompt;
        saveSettings();
        emit systemPromptChanged();
    }
}

bool SettingsManager::webSearch() const
{
    return m_webSearch;
}

void SettingsManager::setWebSearch(bool enabled)
{
    if (m_webSearch != enabled) {
        m_webSearch = enabled;
        saveSettings();
        emit webSearchChanged();
    }
}

bool SettingsManager::voiceAnonymization() const
{
    return m_voiceAnonymization;
}

void SettingsManager::setVoiceAnonymization(bool enabled)
{
    if (m_voiceAnonymization != enabled) {
        m_voiceAnonymization = enabled;
        saveSettings();
        emit voiceAnonymizationChanged();
    }
}

double SettingsManager::voicePitchFactor() const
{
    return m_voicePitchFactor;
}

void SettingsManager::setVoicePitchFactor(double factor)
{
    // Clamp to a sane range: 0.8 (noticeably lower) .. 1.25 (higher).
    // 1.0 means "no pitch change".
    if (factor < 0.8) factor = 0.8;
    if (factor > 1.25) factor = 1.25;
    if (!qFuzzyCompare(m_voicePitchFactor, factor)) {
        m_voicePitchFactor = factor;
        saveSettings();
        emit voicePitchFactorChanged();
    }
}

QString SettingsManager::chatStyle() const
{
    return m_chatStyle;
}

void SettingsManager::setChatStyle(const QString &style)
{
    if (!availableChatStyles().contains(style)) {
        return;
    }
    if (m_chatStyle != style) {
        m_chatStyle = style;
        saveSettings();
        emit chatStyleChanged();
    }
}

QString SettingsManager::autoTitleMode() const
{
    return m_autoTitleMode;
}

void SettingsManager::setAutoTitleMode(const QString &mode)
{
    if (!availableAutoTitleModes().contains(mode)) {
        return;
    }
    if (m_autoTitleMode != mode) {
        m_autoTitleMode = mode;
        saveSettings();
        emit autoTitleModeChanged();
    }
}

bool SettingsManager::showTimestamps() const
{
    return m_showTimestamps;
}

void SettingsManager::setShowTimestamps(bool show)
{
    if (m_showTimestamps != show) {
        m_showTimestamps = show;
        saveSettings();
        emit showTimestampsChanged();
    }
}

bool SettingsManager::enterKeySends() const
{
    return m_enterKeySends;
}

void SettingsManager::setEnterKeySends(bool sends)
{
    if (m_enterKeySends != sends) {
        m_enterKeySends = sends;
        saveSettings();
        emit enterKeySendsChanged();
    }
}

QVariantList SettingsManager::savedPrompts() const
{
    return m_savedPrompts;
}

QString SettingsManager::appVersion() const
{
    return QString::fromLatin1(APP_VERSION);
}

void SettingsManager::addSavedPrompt(const QString &title, const QString &text)
{
    if (text.trimmed().isEmpty() || m_savedPrompts.count() >= MAX_SAVED_PROMPTS) {
        return;
    }

    QVariantMap entry;
    entry["title"] = title.trimmed().isEmpty() ? text.trimmed().left(40) : title.trimmed();
    entry["text"] = text.trimmed();
    m_savedPrompts.prepend(entry);

    saveSavedPrompts();
    emit savedPromptsChanged();
}

void SettingsManager::updateSavedPrompt(int index, const QString &title, const QString &text)
{
    if (index < 0 || index >= m_savedPrompts.count() || text.trimmed().isEmpty()) {
        return;
    }

    QVariantMap entry;
    entry["title"] = title.trimmed().isEmpty() ? text.trimmed().left(40) : title.trimmed();
    entry["text"] = text.trimmed();
    m_savedPrompts[index] = entry;

    saveSavedPrompts();
    emit savedPromptsChanged();
}

void SettingsManager::removeSavedPrompt(int index)
{
    if (index < 0 || index >= m_savedPrompts.count()) {
        return;
    }

    m_savedPrompts.removeAt(index);
    saveSavedPrompts();
    emit savedPromptsChanged();
}

QVariantMap SettingsManager::modelPricing(const QString &modelId) const
{
    QVariantMap pricing;
    pricing["input"] = 0.0;
    pricing["output"] = 0.0;
    pricing["known"] = false;

    const QString id = modelId.toLower();
    for (int i = 0; i < PRICE_COUNT; ++i) {
        if (id.contains(QString::fromLatin1(PRICES[i].match))) {
            pricing["input"] = PRICES[i].inputPerMillion;
            pricing["output"] = PRICES[i].outputPerMillion;
            pricing["known"] = true;
            break;
        }
    }

    return pricing;
}

double SettingsManager::estimatedCost(const QString &modelId,
                                      qint64 promptTokens,
                                      qint64 completionTokens) const
{
    const QVariantMap pricing = modelPricing(modelId);
    if (!pricing["known"].toBool()) {
        return -1.0;
    }

    return (promptTokens / 1000000.0) * pricing["input"].toDouble()
         + (completionTokens / 1000000.0) * pricing["output"].toDouble();
}

QStringList SettingsManager::availableModels() const
{
    // Fallback until the catalogue has been fetched once.
    if (!m_cachedModels.isEmpty()) {
        return m_cachedModels;
    }
    return Providers::fallbackModels();
}

QStringList SettingsManager::availableAutoTitleModes() const
{
    return QStringList()
        << "local"  // first question as title, category from the local classifier
        << "ai"     // one extra request per conversation, best titles
        << "off";   // no title, no category
}

QStringList SettingsManager::availableChatStyles() const
{
    return QStringList()
        << "flat"       // full width tinted rows (historical look)
        << "bubbles"    // rounded bubbles aligned left/right
        << "compact"    // dense rows with a role prefix
        << "cards";     // separated cards with a role header
}

bool SettingsManager::isVisionModel(const QString &modelId) const
{
    if (!m_cachedModels.isEmpty()) {
        return m_cachedVisionModels.contains(modelId);
    }
    return Providers::looksLikeVisionModel(modelId);
}

QString SettingsManager::resolveModel(const QString &candidate) const
{
    if (candidate.isEmpty()) {
        return m_modelName;
    }
    // Nothing fetched yet: no grounds to reject anything.
    if (m_cachedModels.isEmpty() || m_cachedModels.contains(candidate)) {
        return candidate;
    }
    return m_modelName;
}

void SettingsManager::updateModelCache(const QVariantList &models)
{
    QStringList ids;
    QStringList visionIds;

    for (const QVariant &modelVariant : models) {
        QVariantMap model = modelVariant.toMap();
        QString id = model["id"].toString();
        if (id.isEmpty()) {
            continue;
        }
        ids.append(id);
        if (model["vision"].toBool()) {
            visionIds.append(id);
        }
    }

    if (ids.isEmpty()) {
        return;
    }

    m_settings.setValue("models/cachedList", ids);
    m_settings.setValue("models/visionList", visionIds);
    m_settings.setValue("models/cacheTimestamp",
                        QDateTime::currentMSecsSinceEpoch() / 1000);

    // A stored model the provider does not serve would fail on every send.
    // Not a model switch the user made, so the stat is left alone.
    if (!ids.contains(m_modelName)) {
        const QString fallback = Providers::defaultModel();
        const QString replacement = ids.contains(fallback) ? fallback : ids.first();
        m_settings.setValue("modelName", replacement);
        m_modelName = replacement;
        emit modelNameChanged();
    }

    m_cachedModels = ids;
    m_cachedVisionModels = visionIds;

    m_settings.sync();
    secureSettingsFile();
    emit availableModelsChanged();
}

bool SettingsManager::modelCacheStale() const
{
    if (m_cachedModels.isEmpty()) {
        return true;
    }
    const qint64 cachedAt = m_settings.value("models/cacheTimestamp", 0).toLongLong();
    const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    return (now - cachedAt) > 24 * 3600;
}

QStringList SettingsManager::availableLanguages() const
{
    // Codes must match the translations/harbour-skrynja-<code>.ts file names
    return QStringList()
        << "en"
        << "da"
        << "fr"
        << "de"
        << "es"
        << "fi"
        << "it"
        << "nb_NO"
        << "sv";
}

void SettingsManager::clearApiKey()
{
    setApiKey(QString());
}

bool SettingsManager::hasApiKey() const
{
    // Mistral requires a key; an empty one means the app is not configured.
    return !m_apiKey.isEmpty();
}

bool SettingsManager::isFirstLaunch() const
{
    // If the flag is set and true, never show again
    if (m_settings.value("firstLaunchComplete", false).toBool()) {
        return false;
    }

    // Show if no API key configured
    return m_apiKey.isEmpty();
}

void SettingsManager::setFirstLaunchComplete()
{
    m_settings.setValue("firstLaunchComplete", true);
    m_settings.sync();
    secureSettingsFile();
}

void SettingsManager::loadSettings()
{
    migrateToSingleProvider();

    // The key is stored obfuscated. deobfuscate() passes a plain value through
    // untouched, which covers a key written while the salt was unavailable.
    m_apiKey.clear();
    const QString storedKey = m_settings.value("apiKeyEnc", "").toString();
    if (!storedKey.isEmpty()) {
        m_apiKey = SecureStore::deobfuscate(storedKey);
        if (m_apiKey.isEmpty()) {
            qWarning() << "Stored API key could not be decoded; it must be entered again";
            m_settings.remove("apiKeyEnc");
        }
    }

    m_modelName = m_settings.value("modelName",
                                   Providers::defaultModel()).toString();
    m_cachedModels = m_settings.value("models/cachedList").toStringList();
    m_cachedVisionModels = m_settings.value("models/visionList").toStringList();

    // Dropped in 2.1: there is no bundled key, so the toggle only ever got in
    // the way. Remove the leftover entry.
    m_settings.remove("useCustomKey");

    m_settings.remove("nextMessageModel");
    m_language = m_settings.value("language", "en").toString();
    m_temperature = m_settings.value("generation/temperature", -1.0).toDouble();
    m_maxTokens = m_settings.value("generation/maxTokens", 0).toInt();
    m_systemPrompt = m_settings.value("generation/systemPrompt", "").toString();
    m_webSearch = m_settings.value("generation/webSearch", false).toBool();
    m_chatStyle = m_settings.value("ui/chatStyle", "flat").toString();
    if (!availableChatStyles().contains(m_chatStyle)) {
        m_chatStyle = "flat";
    }
    m_autoTitleMode = m_settings.value("generation/autoTitleMode", "local").toString();
    if (!availableAutoTitleModes().contains(m_autoTitleMode)) {
        m_autoTitleMode = "local";
    }
    m_showTimestamps = m_settings.value("ui/showTimestamps", true).toBool();
    m_voiceAnonymization = m_settings.value("voice/anonymization", false).toBool();
    m_voicePitchFactor = m_settings.value("voice/pitchFactor", 1.15).toDouble();
    // 2.3.1 stored "ui/enterKeySends" = true for everyone, chosen or not.
    // A new key resets it once so Enter goes back to inserting a new line.
    m_settings.remove("ui/enterKeySends");
    m_enterKeySends = m_settings.value("ui/enterSends", false).toBool();
    m_modelSwitches = m_settings.value("stats/modelSwitches", 0).toInt();
    m_savedPrompts.clear();
    QJsonDocument promptsDoc = QJsonDocument::fromJson(
                m_settings.value("prompts/library").toByteArray());
    if (promptsDoc.isArray()) {
        const QJsonArray array = promptsDoc.array();
        for (int i = 0; i < array.count(); ++i) {
            const QJsonObject obj = array.at(i).toObject();
            const QString text = obj["text"].toString();
            if (text.isEmpty()) {
                continue;
            }
            QVariantMap entry;
            entry["title"] = obj["title"].toString();
            entry["text"] = text;
            m_savedPrompts.append(entry);
        }
    }
    m_settings.sync();
}

void SettingsManager::migrateToSingleProvider()
{
    // SailCat stored the key, the model and the model cache under
    // providers/mistral/ (or, before 2.3, at the top level). Move them to
    // single-provider keys once. The pre-2.1 clear-text entry is still read:
    // an upgrade that skipped 2.1 and 2.2 must not lose a configured key.
    if (m_settings.value("apiKeyEnc").isValid()
            || m_settings.value("modelName").isValid()) {
        return;
    }

    QString key = m_settings.value("providers/mistral/apiKeyEnc", "").toString();
    if (key.isEmpty()) {
        key = m_settings.value("apiKeyEnc", "").toString();
    }
    if (key.isEmpty()) {
        const QString legacy = m_settings.value("apiKey", "").toString();
        if (!legacy.isEmpty()) {
            key = SecureStore::isAvailable() ? SecureStore::obfuscate(legacy) : legacy;
        }
    }
    if (!key.isEmpty()) {
        m_settings.setValue("apiKeyEnc", key);
    }

    const QString model = m_settings.value("providers/mistral/modelName",
                                           m_settings.value("modelName", "")).toString();
    if (!model.isEmpty()) {
        m_settings.setValue("modelName", model);
    }

    const QStringList cached = m_settings.value("providers/mistral/models/cachedList",
                                                m_settings.value("models/cachedList").toStringList())
            .toStringList();
    if (!cached.isEmpty()) {
        m_settings.setValue("models/cachedList", cached);
        m_settings.setValue("models/visionList",
                            m_settings.value("providers/mistral/models/visionList",
                                             m_settings.value("models/visionList").toStringList())
                            .toStringList());
        m_settings.setValue("models/cacheTimestamp",
                            m_settings.value("providers/mistral/models/cacheTimestamp",
                                             m_settings.value("models/cacheTimestamp", 0).toLongLong())
                            .toLongLong());
    }

    // The multi-provider entries are dead weight now. The migrated
    // single-provider keys (apiKeyEnc, modelName, models/) stay.
    m_settings.remove("provider");
    m_settings.remove("providers");
    m_settings.remove("apiKey");
    m_settings.sync();
    secureSettingsFile();
}

void SettingsManager::persistApiKey()
{
    if (m_apiKey.isEmpty()) {
        m_settings.remove("apiKeyEnc");
        return;
    }

    // Without a persisted salt the ciphertext would be unreadable on the next
    // launch, so store the key as-is rather than destroy it. The file is still
    // owner-only either way.
    m_settings.setValue("apiKeyEnc", SecureStore::isAvailable()
                        ? SecureStore::obfuscate(m_apiKey)
                        : m_apiKey);
}

void SettingsManager::saveSettings()
{
    persistApiKey();
    m_settings.setValue("modelName", m_modelName);
    m_settings.remove("nextMessageModel");
    m_settings.setValue("language", m_language);
    m_settings.setValue("generation/temperature", m_temperature);
    m_settings.setValue("generation/maxTokens", m_maxTokens);
    m_settings.setValue("generation/systemPrompt", m_systemPrompt);
    m_settings.setValue("generation/webSearch", m_webSearch);
    m_settings.setValue("generation/autoTitleMode", m_autoTitleMode);
    m_settings.setValue("ui/chatStyle", m_chatStyle);
    m_settings.setValue("ui/showTimestamps", m_showTimestamps);
    m_settings.setValue("ui/enterSends", m_enterKeySends);
    m_settings.setValue("voice/anonymization", m_voiceAnonymization);
    m_settings.setValue("voice/pitchFactor", m_voicePitchFactor);
    m_settings.sync();
    secureSettingsFile();
}

void SettingsManager::saveSavedPrompts()
{
    QJsonArray array;
    for (const QVariant &entry : m_savedPrompts) {
        const QVariantMap map = entry.toMap();
        QJsonObject obj;
        obj["title"] = map["title"].toString();
        obj["text"] = map["text"].toString();
        array.append(obj);
    }

    m_settings.setValue("prompts/library",
                        QJsonDocument(array).toJson(QJsonDocument::Compact));
    m_settings.sync();
    secureSettingsFile();
}

void SettingsManager::secureSettingsFile()
{
    SecureStore::restrictPermissions(m_settings.fileName());
}
