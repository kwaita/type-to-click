#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cctype>
#include <string>

using namespace geode::prelude;

namespace {
	bool g_syntheticInput = false;

	struct SyntheticInputGuard final {
		SyntheticInputGuard() { g_syntheticInput = true; }
		~SyntheticInputGuard() { g_syntheticInput = false; }
	};

	std::string lower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return s;
	}

	bool endsWith(std::string const& s, std::string const& suffix) {
		if (suffix.empty() || s.size() < suffix.size()) return false;
		return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
	}

	enum class Action {
		Click,
		Hold,
		Release,
		TempoHold,
	};

	bool keyToChar(int k, char& out) {
		if (k >= 0x41 && k <= 0x5A) {
			out = static_cast<char>('a' + (k - 0x41));
			return true;
		}
		if (k >= 0x30 && k <= 0x39) {
			out = static_cast<char>('0' + (k - 0x30));
			return true;
		}
		return false;
	}
}

class $modify(TypeToClickGameLayer, GJBaseGameLayer) {
	void handleButton(bool down, int button, bool isPlayer1) {
		if (!Mod::get()->getSettingValue<bool>("enabled") || g_syntheticInput) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		if (button != static_cast<int>(PlayerButton::Jump)) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}
	}
};

class $modify(TypeToClickPlayLayer, PlayLayer) {
	struct Fields {
		std::string m_buffer;

		bool m_jumpDown = false;
		int m_releaseAfterFrames = -1;
		float m_releaseAfterSeconds = -1.f;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		return true;
	}

	void update(float dt) {
		PlayLayer::update(dt);

		if (!Mod::get()->getSettingValue<bool>("enabled")) return;

		if (m_fields->m_releaseAfterFrames >= 0) {
			if (m_fields->m_releaseAfterFrames == 0) {
				this->releaseJump();
				m_fields->m_releaseAfterFrames = -1;
			} else {
				m_fields->m_releaseAfterFrames -= 1;
			}
		}

		if (m_fields->m_releaseAfterSeconds >= 0.f) {
			m_fields->m_releaseAfterSeconds -= dt;
			if (m_fields->m_releaseAfterSeconds <= 0.f) {
				this->releaseJump();
				m_fields->m_releaseAfterSeconds = -1.f;
			}
		}
	}

	void keyDown(enumKeyCodes key, double dt) {
		PlayLayer::keyDown(key, dt);

		if (!Mod::get()->getSettingValue<bool>("enabled")) return;

		auto k = static_cast<int>(key);

		if (k == 0x08) {
			this->onBackspace();
			return;
		}

		if (k == 0x20 || k == 0x0D || k == 0x1B) {
			this->clearBuffer();
			return;
		}

		char c = 0;
		if (keyToChar(k, c)) {
			this->onCharTyped(c);
		}
	}

	void handleAction(Action action) {
		if (!Mod::get()->getSettingValue<bool>("enabled")) return;

		switch (action) {
			case Action::Click:
				this->pressJump();
				m_fields->m_releaseAfterFrames = 1;
				m_fields->m_releaseAfterSeconds = -1.f;
				break;

			case Action::Hold:
				this->pressJump();
				m_fields->m_releaseAfterFrames = -1;
				m_fields->m_releaseAfterSeconds = -1.f;
				break;

			case Action::Release:
				this->releaseJump();
				m_fields->m_releaseAfterFrames = -1;
				m_fields->m_releaseAfterSeconds = -1.f;
				break;

			case Action::TempoHold:
				this->pressJump();
				m_fields->m_releaseAfterFrames = -1;
				m_fields->m_releaseAfterSeconds = 1.f;
				break;
		}
	}

	void clearBuffer() {
		m_fields->m_buffer.clear();
	}

	void onCharTyped(char c) {
		if (!Mod::get()->getSettingValue<bool>("enabled")) return;

		if (std::isprint(static_cast<unsigned char>(c)) == 0) return;
		m_fields->m_buffer.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		if (m_fields->m_buffer.size() > 128) {
			m_fields->m_buffer.erase(0, m_fields->m_buffer.size() - 128);
		}

		auto clickWord = lower(Mod::get()->getSettingValue<std::string>("click-word"));
		auto holdWord = lower(Mod::get()->getSettingValue<std::string>("hold-word"));
		auto releaseWord = lower(Mod::get()->getSettingValue<std::string>("release-word"));
		auto tempoHoldWord = lower(Mod::get()->getSettingValue<std::string>("tempo-hold-word"));

		if (endsWith(m_fields->m_buffer, clickWord)) {
			this->handleAction(Action::Click);
			this->clearBuffer();
		} else if (endsWith(m_fields->m_buffer, holdWord)) {
			this->handleAction(Action::Hold);
			this->clearBuffer();
		} else if (endsWith(m_fields->m_buffer, releaseWord)) {
			this->handleAction(Action::Release);
			this->clearBuffer();
		} else if (endsWith(m_fields->m_buffer, tempoHoldWord)) {
			this->handleAction(Action::TempoHold);
			this->clearBuffer();
		}
	}

	void onBackspace() {
		if (!Mod::get()->getSettingValue<bool>("enabled")) return;
		if (!m_fields->m_buffer.empty()) m_fields->m_buffer.pop_back();
	}

	void pressJump() {
		if (m_fields->m_jumpDown) return;
		m_fields->m_jumpDown = true;
		SyntheticInputGuard guard;
		GJBaseGameLayer::handleButton(true, static_cast<int>(PlayerButton::Jump), true);
	}

	void releaseJump() {
		if (!m_fields->m_jumpDown) return;
		m_fields->m_jumpDown = false;
		SyntheticInputGuard guard;
		GJBaseGameLayer::handleButton(false, static_cast<int>(PlayerButton::Jump), true);
	}
};
