#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <argon/argon.hpp>
#include <memory>

using namespace geode::prelude;

bool g_isPlaying = false;
bool g_isEditing = false;
std::string g_levelName = "Main Menu";
int g_percentage = 0;
std::string g_authToken = "";
int g_authedAccountId = 0; 
 
void ensureAuthenticated() {
    auto accountManager = GJAccountManager::sharedState();
    if (!accountManager || accountManager->m_accountID == 0) return;
    int currentAccountId = accountManager->m_accountID;  
    if (!g_authToken.empty() && g_authedAccountId == currentAccountId) return;

    
    g_authToken = "";
    g_authedAccountId = 0;

    int capturedId = currentAccountId;
    async::spawn(
        argon::startAuth(),
        [capturedId](Result<std::string> result) {
            if (result.isOk()) {
               
                auto am = GJAccountManager::sharedState();
                if (am && am->m_accountID == capturedId) {
                    g_authToken = std::move(result).unwrap();
                    g_authedAccountId = capturedId;
                    log::info("Argon authentication successful for account {}", capturedId);
                }
            }
            else {
                log::warn("Argon authentication failed: {}", result.unwrapErr());
            }
        }
    );
}

void sendPlayerStatus(bool isPlaying, const std::string& levelName = "", int percentage = 0, bool isEditing = false) {
    g_isPlaying = isPlaying;
    g_isEditing = isEditing;
    g_levelName = levelName;
    g_percentage = percentage;

    
    ensureAuthenticated();

    if (g_authToken.empty()) return;
    auto accountManager = GJAccountManager::sharedState();
    if (!accountManager || accountManager->m_accountID == 0) return;
    if (accountManager->m_accountID != g_authedAccountId) return;

    matjson::Value payload;
    payload["accountId"] = std::to_string(accountManager->m_accountID);
    payload["username"] = accountManager->m_username.c_str();
    payload["levelName"] = levelName;
    payload["isPlaying"] = isPlaying;
    payload["isEditing"] = isEditing;
    payload["percentage"] = percentage;
    payload["authtoken"] = g_authToken;

    std::string url = "https://player-status-server.onrender.com/update-status";

    auto req = web::WebRequest();
    req.certVerification(false);
    req.bodyJSON(payload);

    async::spawn(
        req.post(url),
        [](web::WebResponse response) {}
    );
}

class StatusPinger : public cocos2d::CCNode {
public:
    void sendPing(float dt) {
        sendPlayerStatus(g_isPlaying, g_levelName, g_percentage, g_isEditing);
    }
};

$on_mod(Loaded) {
    auto pinger = new StatusPinger();
    cocos2d::CCDirector::sharedDirector()->getScheduler()->scheduleSelector(
        schedule_selector(StatusPinger::sendPing),
        pinger,
        15.0f,
        false
    );
}

class $modify(StatusPlayLayer, PlayLayer) {

    struct Fields {
        int lastSentPercent = 0;
    };

    bool init(GJGameLevel * level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        int bestPercent = level->m_normalPercent.value();
        if (bestPercent > 100) bestPercent = 100;
        m_fields->lastSentPercent = bestPercent;
        sendPlayerStatus(true, level->m_levelName, bestPercent);

        this->schedule(schedule_selector(StatusPlayLayer::checkNewBest), 3.0f);
        return true;
    }

    void checkNewBest(float dt) {
        if (!m_level) return;
        int currentBest = m_level->m_normalPercent.value();
        if (currentBest > 100) currentBest = 100;

        if (currentBest > m_fields->lastSentPercent) {
            m_fields->lastSentPercent = currentBest;
            sendPlayerStatus(true, m_level->m_levelName, currentBest);
        }
    }

    void onQuit() {
        this->unschedule(schedule_selector(StatusPlayLayer::checkNewBest));
        g_percentage = 0;
        sendPlayerStatus(false, "Menu");
        PlayLayer::onQuit();
    }
};

class $modify(StatusEditorLayer, LevelEditorLayer) {

    bool init(GJGameLevel * level, bool p1) {
        if (!LevelEditorLayer::init(level, p1)) return false;

        std::string name = level->m_levelName;
        sendPlayerStatus(false, name, 0, true);

        return true;
    }
};

class $modify(StatusEditorPause, EditorPauseLayer) {

    void onExitEditor(cocos2d::CCObject * sender) {
        sendPlayerStatus(false, "Menu");
        EditorPauseLayer::onExitEditor(sender);
    }
};

class $modify(StatusProfilePage, ProfilePage) {

    struct Fields {
        std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
        cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
        cocos2d::CCLabelBMFont* m_percentLabel = nullptr;
        int m_targetAccountId = 0;

        ~Fields() {
            *m_alive = false;
        }
    };

    void loadPageFromUserInfo(GJUserScore * score) {
        ProfilePage::loadPageFromUserInfo(score);

        auto layer = this->m_mainLayer;
        if (!layer) return;

        m_fields->m_targetAccountId = score->m_accountID;

     
        auto accountManager = GJAccountManager::sharedState();
        if (accountManager && accountManager->m_accountID == score->m_accountID) {
            sendPlayerStatus(g_isPlaying, g_levelName, g_percentage);
        }

        
        if (auto oldStatus = layer->getChildByID("player-status-label"_spr)) {
            oldStatus->removeFromParentAndCleanup(true);
        }
        if (auto oldPercent = layer->getChildByID("player-percent-label"_spr)) {
            oldPercent->removeFromParentAndCleanup(true);
        }
        m_fields->m_statusLabel = nullptr;
        m_fields->m_percentLabel = nullptr;

      
        this->unschedule(schedule_selector(StatusProfilePage::updateStatus));

       
        cocos2d::CCNode* usernameNode = layer->getChildByIDRecursive("username-menu");

        float xPos;
        float yPos;

        if (usernameNode && usernameNode->getParent()) {
   
            auto worldPos = usernameNode->getParent()->convertToWorldSpace(usernameNode->getPosition());
            auto localPos = layer->convertToNodeSpace(worldPos);
            xPos = localPos.x;
            yPos = localPos.y - 14.0f;
        }
        else {
       
            auto contentSize = layer->getContentSize();
            xPos = contentSize.width / 2;
            yPos = contentSize.height * 0.85f;
        }

        auto statusLabel = cocos2d::CCLabelBMFont::create("Searching...", "bigFont.fnt");
        statusLabel->setID("player-status-label"_spr);
        statusLabel->setScale(0.5f);
        statusLabel->setAnchorPoint({ 0.5f, 0.5f });
        statusLabel->setPosition({ xPos, yPos });
        layer->addChild(statusLabel);

        auto percentLabel = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        percentLabel->setID("player-percent-label"_spr);
        percentLabel->setScale(0.5f);
        percentLabel->setColor({ 255, 255, 50 });
        percentLabel->setAnchorPoint({ 0.0f, 0.5f });
        percentLabel->setPosition({ xPos, yPos });
        percentLabel->setVisible(false);
        layer->addChild(percentLabel);

        m_fields->m_statusLabel = statusLabel;
        m_fields->m_percentLabel = percentLabel;

        this->updateStatus(0.0f);
        this->schedule(schedule_selector(StatusProfilePage::updateStatus), 5.0f);
    }

    void updateStatus(float dt) {
        if (!m_fields->m_statusLabel) return;

        auto label = m_fields->m_statusLabel;
        auto percentLabel = m_fields->m_percentLabel;

     
        std::string url = "https://player-status-server.onrender.com/get-status/" + std::to_string(m_fields->m_targetAccountId);
        auto req = web::WebRequest();
        req.certVerification(false);

        auto alive = m_fields->m_alive;

        async::spawn(
            req.get(url),
            [alive, label, percentLabel](web::WebResponse response) {
                if (!(*alive)) return;

                auto reposition = [&]() {
                    label->setAnchorPoint({ 1.0f, 0.5f });
               
                    auto parent = label->getParent();
                    float parentWidth = parent ? parent->getContentSize().width
                        : cocos2d::CCDirector::sharedDirector()->getWinSize().width;

                    float statusW = label->getContentSize().width * label->getScale();
                    float percentW = percentLabel->isVisible() ? percentLabel->getContentSize().width * percentLabel->getScale() : 0;
                    float totalW = statusW + percentW;
                    float startX = (parentWidth - totalW) / 2.0f;
                    label->setPosition({ startX + statusW, label->getPositionY() });
                    percentLabel->setPosition({ startX + statusW, percentLabel->getPositionY() });
                    };

                if (response.ok()) {
                    auto jsonResult = response.json();
                    if (jsonResult.isOk()) {
                        auto json = jsonResult.unwrap();
                        std::string status = json["status"].asString().unwrapOr("offline");

                        if (status == "online") {
                            bool isPlaying = json["isPlaying"].asBool().unwrapOr(false);
                            bool isEditing = json["isEditing"].asBool().unwrapOr(false);
                            std::string level = json["levelName"].asString().unwrapOr("");
                            int percent = json["percentage"].asInt().unwrapOr(0);

                            if (isEditing) {
                                label->setString("Editor: ");
                                label->setColor({ 180, 80, 255 });  
                                percentLabel->setString(level.c_str());
                                percentLabel->setColor({ 255, 255, 50 });  
                                percentLabel->setVisible(true);
                            }
                            else if (isPlaying) {
                                label->setString(("Playing: " + level + " - ").c_str());
                                label->setColor({ 50, 200, 255 });
                                percentLabel->setString((std::to_string(percent) + "%").c_str());
                                percentLabel->setVisible(true);
                            }
                            else {
                                label->setString("Online");
                                label->setColor({ 50, 255, 50 });
                                percentLabel->setVisible(false);
                            }
                        }
                        else {
                            label->setString("Offline");
                            label->setColor({ 160, 160, 160 });
                            percentLabel->setVisible(false);
                        }
                    }
                }
                else {
                    label->setString("Network error");
                    label->setColor({ 255, 70, 70 });
                    percentLabel->setVisible(false);
                }

                reposition();
            }
        );
    }
};