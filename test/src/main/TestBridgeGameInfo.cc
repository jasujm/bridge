#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "MockCardManager.hh"
#include "MockBridgeGameInfo.hh"

#include <gtest/gtest.h>
#include <memory>

class BridgeGameInfoTest : public testing::Test {
public:
    std::shared_ptr<Bridge::Engine::CardManager> cardManager {
        std::make_shared<testing::NiceMock<Bridge::Engine::MockCardManager>>()};
    std::shared_ptr<Bridge::Engine::DuplicateGameManager> gameManager {
        std::make_shared<Bridge::Engine::DuplicateGameManager>()};
    Bridge::Engine::BridgeEngine engine {cardManager, gameManager};
    Bridge::Main::MockBridgeGameInfo gameInfo;
};

using testing::ReturnRef;
using testing::ReturnPointee;

TEST_F(BridgeGameInfoTest, testGetEngine) {
    EXPECT_CALL(gameInfo, handleGetEngine()).WillOnce(ReturnRef(engine));
    EXPECT_EQ(&engine, &gameInfo.getEngine());
}

TEST_F(BridgeGameInfoTest, testGetGameManager) {
    EXPECT_CALL(gameInfo, handleGetGameManager())
        .WillOnce(ReturnPointee(gameManager));
    EXPECT_EQ(gameManager.get(), &gameInfo.getGameManager());
}
