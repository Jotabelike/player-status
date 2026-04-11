#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <memory>

using namespace geode::prelude;

 
bool g_isPlaying = false;
std::string g_levelName = "Main Menu";
int g_percentage = 0;

 
void sendPlayerStatus(bool isPlaying, const std::string& levelName = "", int percentage = 0) {
    g_isPlaying = isPlaying;
    g_levelName = levelName;
    g_percentage = percentage;

    auto accountManager = GJAccountManager::sharedState();
    if (!accountManager || accountManager->m_accountID == 0) return;

    matjson::Value payload;
    payload["accountId"] = std::to_string(accountManager->m_accountID);
    payload["username"] = accountManager->m_username.c_str();
    payload["levelName"] = levelName;
    payload["isPlaying"] = isPlaying;
    payload["percentage"] = percentage;

    std::string url = "https://player-status-server.onrender.com/update-status";

    auto req = web::WebRequest();
    req.certVerification(false);
    req.bodyJSON(payload);

    geode::async::spawn(
        req.post(url),
        [](web::WebResponse response) {  }
    );
}

 
class StatusPinger : public cocos2d::CCNode {
public:
    void sendPing(float dt) {
        sendPlayerStatus(g_isPlaying, g_levelName, g_percentage);
    }
};

$on_mod(Loaded) {
    sendPlayerStatus(false, "Menu");

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

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        float yPos = winSize.height - 54.0f;

        auto statusLabel = cocos2d::CCLabelBMFont::create("Searching...", "bigFont.fnt");
        statusLabel->setID("player-status-label"_spr);
        statusLabel->setScale(0.5f);
        statusLabel->setAnchorPoint({ 0.5f, 0.5f });  
        statusLabel->setPosition({ winSize.width / 2, yPos });
        layer->addChild(statusLabel, 100);

        auto percentLabel = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        percentLabel->setID("player-percent-label"_spr);
        percentLabel->setScale(0.5f);
        percentLabel->setColor({ 255, 255, 50 });
        percentLabel->setAnchorPoint({ 0.0f, 0.5f });  
        percentLabel->setPosition({ winSize.width / 2, yPos });
        percentLabel->setVisible(false);
        layer->addChild(percentLabel, 100);

        m_fields->m_statusLabel = statusLabel;
        m_fields->m_percentLabel = percentLabel;

        this->updateStatus(0.0f);
        this->schedule(schedule_selector(StatusProfilePage::updateStatus), 5.0f);
    }

    void updateStatus(float dt) {
        if (!m_fields->m_statusLabel) return;

        std::string url = "https://player-status-server.onrender.com/get-status/" + std::to_string(m_fields->m_targetAccountId);
        auto req = web::WebRequest();
        req.certVerification(false);

        auto alive = m_fields->m_alive;
        auto label = m_fields->m_statusLabel;
        auto percentLabel = m_fields->m_percentLabel;

        geode::async::spawn(
            req.get(url),
            [alive, label, percentLabel](web::WebResponse response) {
                if (!(*alive)) return;

                auto reposition = [&]() {
                    label->setAnchorPoint({ 1.0f, 0.5f });
                    auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
                    float statusW = label->getContentSize().width * label->getScale();
                    float percentW = percentLabel->isVisible() ? percentLabel->getContentSize().width * percentLabel->getScale() : 0;
                    float totalW = statusW + percentW;
                    float startX = (winSize.width - totalW) / 2.0f;
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
                            std::string level = json["levelName"].asString().unwrapOr("");
                            int percent = json["percentage"].asInt().unwrapOr(0);

                            if (isPlaying) {
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