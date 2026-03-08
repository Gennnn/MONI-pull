#include "DrawComponents.h"
#include "../GAME/GameComponents.h"
#include "./Utility/BinaryReader.h"
#include "./Utility/TextureUtils.h"
#include "./Utility/CacheUtil.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"
#include "../GAME/SaveScore.h"
#include "../GAME/AudioComponents.h"
#include "../GAME/Timer.h"

namespace DRAW {

	/*
	
	GENERAL UI UTIL FUNCTIONS
	
	*/
	static inline float AlignFactor(HorizontalAlignment horizontalAlignment) {
		switch (horizontalAlignment) {
		default:
		case HorizontalAlignment::LEFT: return 0.0f;
		case HorizontalAlignment::CENTER: return 0.5f;
		case HorizontalAlignment::RIGHT: return 1.0f;
		}
	}

	static inline float AlignFactor(VerticalAlignment verticalAlignment) {
		switch (verticalAlignment) {
		default:
		case VerticalAlignment::TOP: return 0.0f;
		case VerticalAlignment::MIDDLE: return 0.5f;
		case VerticalAlignment::BOTTOM: return 1.0f;
		}
	}

	static inline unsigned int PackRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		return (unsigned int)r | ((unsigned int)g << 8) | ((unsigned int)b << 16) | ((unsigned int)a << 24);
	}

	static inline float2 add(float2 a, float2 b) {
		return { a.x + b.x, a.y + b.y };
	}

	static inline float2 subtract(float2 a, float2 b) {
		return {a.x - b.x, a.y - b.y};
	}

	static inline float2 multiply(float2 a, float2 b) {
		return { a.x * b.x, a.y * b.y };
	}

	static inline float ComputeFit(unsigned int width, unsigned int height, unsigned int referenceWidth, unsigned int referenceHeight) {
		double fit = min( (double)width / (double)referenceWidth, (double)height / (double)referenceHeight);
		static const float scaleSnaps[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};
		float bestFit = scaleSnaps[0];
		float bestDifference = fabsf(fit - bestFit);

		for (float snap : scaleSnaps) {
			float d = fabsf(fit - snap);
			if (d < bestDifference) { 
				bestDifference = d;
				bestFit = snap;
			}
		}

		return bestFit;
	}

	/*
	
	TEXT UTIL FUNCTIONS
	
	*/
	static inline float GetCharacterHorizontalAdvance(const UIFont& font, char character) {
		if (character < font.firstCharacter || character >= font.firstCharacter + font.numberOfCharacters) return 0.0f;
		int characterIndex = ((int)character) - font.firstCharacter;
		return font.bakedCharacters[characterIndex].xadvance;
	}


	static inline TextBlockInfo CalculateTextBlockInfo(const UIFont& font, const char* text) {
		TextBlockInfo info{};
		float currentWidth = 0.0f;

		info.lineWidths.clear();
		info.lineWidths.push_back(0.0f);

		for (const char* currentChar = text; *currentChar; currentChar++) {
			char character = *currentChar;
			if (character == '\r') continue;
			if (character == '\n') {
				info.lineWidths.back() = currentWidth;
				info.textBlockWidth = max(info.textBlockWidth, currentWidth);
				currentWidth = 0.0f;
				info.lineWidths.push_back(0.0f);
				continue;
			}
			currentWidth += GetCharacterHorizontalAdvance(font, character);
		}

		info.lineWidths.back() = currentWidth;
		info.textBlockWidth = max(info.textBlockWidth, currentWidth);

		float lines = (float)info.lineWidths.size();
		info.textBlockHeight = lines * font.lineHeight;
		return info;
	}

	static inline bool ParseFontName(const std::string& fontName, std::string& outPath, int& outSize) {
		size_t delimiterLocation = fontName.rfind('|');
		if (delimiterLocation == std::string::npos) return false;

		outPath = fontName.substr(0, delimiterLocation);
		outSize = std::atoi(fontName.substr(delimiterLocation + 1).c_str());
		return outSize > 0;
	}

	static inline std::string MakeScaledFontKey(const std::string& fontName, float uiScale) {
		std::string path;
		int baseSize = 0;

		if (!ParseFontName(fontName, path, baseSize)) return fontName;

		int scaledSize = (int)std::round(baseSize * uiScale);

		if (scaledSize < 8) scaledSize = 8;

		return path + "|" + std::to_string(scaledSize);
	}

	static inline std::string FormatScore(int score, int width = 6) {
		std::ostringstream oss;
		oss << std::setw(width) << std::setfill(' ') << score;
		return oss.str();
	}

	/*

	TREASURE CHECK HELPER

	*/
	//god awful O(n) function it's so hacky but the ui needs to be done
	static inline bool HasTreasure(const std::unordered_map<int, int>& levelAndTreasure, int treasureId) {
		for (const auto& kv : levelAndTreasure) {
			if (kv.second == treasureId) return true;
		}
		return false;
	}

	


	/*
	
	ANIMATOR UTIL FUNCTIONS
	
	*/

	static inline float2 ConvertToScreenCoords(const RectTransform& rectTransform, float windowWidth, float windowHeight) {
		return {
			rectTransform.anchor.x * windowWidth + rectTransform.offset.x,
			rectTransform.anchor.y * windowHeight + rectTransform.offset.y
		};
	}

	static inline void MakeAlignedRect(float2 anchorPosition, float2 extents, HorizontalAlignment horizontalAlignment, VerticalAlignment verticalAlignment, float2& outTopLeft, float2& outBottomRight) {
		float horizontalFactor = AlignFactor(horizontalAlignment);
		float verticalFactor = AlignFactor(verticalAlignment);

		outTopLeft = { anchorPosition.x - extents.x * horizontalFactor, anchorPosition.y - extents.y * verticalFactor };
		outBottomRight = { outTopLeft.x + extents.x, outTopLeft.y + extents.y };
	}

	static inline RectTransform BuildRectTransform(float x, float y, float offsetX, float offsetY) {
		return RectTransform{ {x,y}, {offsetX,offsetY} };
	}

	static inline void ResolveElementWorldLocation(std::vector<UIElement>& elements, std::unordered_map<std::string, int>& idAndIndex, int index, float2 windowSize, std::vector<uint8_t>& state, float uiScale) {
		if (state[index] != 0) return;

		state[index] = 1;
		UIElement& element = elements[index];

		float2 parentTopLeft{};
		float2 parentSize = windowSize;

		if (!element.parentId.empty()) {
			auto iterator = idAndIndex.find(element.parentId);
			if (iterator != idAndIndex.end()) {
				int parentIndex = iterator->second;
				ResolveElementWorldLocation(elements, idAndIndex, parentIndex, windowSize, state, uiScale);

				UIElement& parent = elements[parentIndex];
				parentTopLeft = parent.topLeft;
				parentSize = subtract(parent.bottomRight, parent.topLeft);
			}
		}

		float2 offset = element.localTransform.offset;
		if (element.scaleFlags & UIElementScaleFlags::SCALE_OFFSET) {
			offset.x *= uiScale;
			offset.y *= uiScale;
		}

		element.position = add(parentTopLeft, add(multiply(element.localTransform.anchor, parentSize), offset));

		float2 extents = element.extents;
		if (element.scaleFlags & UIElementScaleFlags::SCALE_EXTENTS_X) extents.x *= uiScale;
		if (element.scaleFlags & UIElementScaleFlags::SCALE_EXTENTS_Y) extents.y *= uiScale;

		element.position.x = std::round(element.position.x);
		element.position.y = std::round(element.position.y);
		extents.x = std::round(extents.x);
		extents.y = std::round(extents.y);

		MakeAlignedRect(element.position, extents, element.alignment.horizontal, element.alignment.vertical, element.topLeft, element.bottomRight);

		state[index] = 2;
	}

	static inline void ResolveUIHierarchy(std::vector<UIElement>& elements, float2 windowSize, float uiScale) {
		std::unordered_map<std::string, int> idAndIndex;
		idAndIndex.reserve(elements.size());
		for (int i = 0; i < elements.size(); i++) {
			idAndIndex[elements[i].id] = i;
		}

		std::vector<uint8_t> state(elements.size(), 0);
		for (int i = 0; i < elements.size(); i++) {
			ResolveElementWorldLocation(elements, idAndIndex, i, windowSize, state, uiScale);
		}
	}

	/*
	
	ANIMATOR FUNCTIONS
	
	*/

	static inline UIElement* GetElementById(std::vector<UIElement>& elements, const std::string& elementId) {
		for (auto& element : elements) {
			if (element.id == elementId) return &element;
		}
		return nullptr;
	}

	static void DispatchUIAnimationEvent(entt::registry& registry, entt::entity entity, UI& ui, const UIAnimationEvent& event) {
		switch (event.eventType) {
		case UIAnimationEventType::PLAY_SOUND:
			AUDIO::playAudio(registry, event.data, 1.0f, 0.0f, 0.9f, 1.1f);
			break;
		case UIAnimationEventType::SET_UI_STATE:
			registry.emplace_or_replace<UIStateChange>(entity, UIStateChange{ event.newState });
			break;
		case UIAnimationEventType::RUN_COMMAND:
			RunCommand(registry, ui, event.commandToRun, event.data);
			break;
		case UIAnimationEventType::ENABLE_SELECTION:
			ui.uiSelector.enabled = true;
			break;
		case UIAnimationEventType::DISABLE_SELECTION:
			ui.uiSelector.enabled = false;
			break;
		case UIAnimationEventType::ACTIVATE_SELECTED:
			ActivateSelected(registry, ui);
			break;
		case UIAnimationEventType::CUSTOM:
			break;
		default:
			break;
		}
	}

	static void FireEventsInRange(entt::registry& registry, entt::entity entity, UI& ui, const AnimationClip& clip, float t0, float t1) {
		for (const auto& event : clip.events) {
			if (event.t > t0 && event.t <= t1) {
				DispatchUIAnimationEvent(registry, entity, ui, event.event);
			}
		}
	}

	static inline float WrapTime(float t, float clipLength) {
		while (t >= clipLength) {
			t -= clipLength;
		}
		while (t < 0.0f) {
			t += clipLength;
		}
		return t;
	}

	static void FireClipEvents(entt::registry& registry, entt::entity entity, UI& ui, const AnimationClip& clip, float previousTime, float currentTime) {
		if (clip.events.empty()) return;
		if (clip.lengthSeconds <= 0.0f) return;
		float a, b;
		if (!clip.loop) {
			a = max(0.0f, min(previousTime, clip.lengthSeconds));
			b = max(0.0f, min(currentTime, clip.lengthSeconds));
		}
		else {
			a = WrapTime(previousTime, clip.lengthSeconds);
			b = WrapTime(currentTime, clip.lengthSeconds);
		}

		if (!clip.loop) {
			if (b > a) {
				FireEventsInRange(registry, entity, ui, clip, a, b);
			}
		}
		else {
			if (b >= a) {
				FireEventsInRange(registry, entity, ui, clip, a, b);
			}
			else {
				FireEventsInRange(registry, entity, ui, clip, a, clip.lengthSeconds);
				FireEventsInRange(registry, entity, ui, clip, -1.0f, b);
			}
		}
	}

	static inline void SwitchToClip(UIAnimator& animator, const std::string& name, float overflowSeconds = 0.0f) {
		auto animationsIter = animator.animations.find(name);
		if (animationsIter == animator.animations.end()) return;
		const AnimationClip* nextClip = &animationsIter->second;

		animator.currentlyPlayingClip = nextClip;

		animator.currentTime = max(0, overflowSeconds);
		if (nextClip->loop && nextClip->lengthSeconds > 0.0f) {
			animator.currentTime = WrapTime(animator.currentTime, nextClip->lengthSeconds);
		}
		else if (nextClip->lengthSeconds > 0.0f) {
			animator.currentTime = min(animator.currentTime, nextClip->lengthSeconds);
		}
	}

	static inline void UpdateUIAnimator(entt::registry& registry, entt::entity entity, UI& ui, UIAnimator& animator, std::vector<UIElement>& elements, float dtSeconds, float2 windowDimensions) {
		if (!animator.currentlyPlayingClip) return;

		
		const AnimationClip& animationClip = *animator.currentlyPlayingClip;

		float previousTime = animator.currentTime;
		animator.currentTime += dtSeconds;
		float currentTime = animator.currentTime;

		FireClipEvents(registry, entity, ui, animationClip, previousTime, currentTime);

		float t = animator.currentTime;
		bool ended = false;

		if (animationClip.loop && animationClip.lengthSeconds > 0.0f) {
			while (t >= animationClip.lengthSeconds) t -= animationClip.lengthSeconds;
		}
		else {
			if (animationClip.lengthSeconds > 0.0f && t > animationClip.lengthSeconds) {
				t = animationClip.lengthSeconds;
				ended = true;
			}
		}

		for (const auto& animationTrack : animationClip.animationTracks) {
			UIElement* elementToAnimate = GetElementById(elements, animationTrack.id);
			if (!elementToAnimate) continue;

			if (!animationTrack.pos.keys.empty()) {
				RectTransform transform = animationTrack.pos.Sample(t);
				elementToAnimate->localTransform = animationTrack.pos.Sample(t);
			}
			if (!animationTrack.color.keys.empty()) elementToAnimate->color = animationTrack.color.Sample(t);
			if (!animationTrack.text.keys.empty()) elementToAnimate->text = animationTrack.text.Sample(t);
			if (!animationTrack.textureName.keys.empty()) elementToAnimate->textureName = animationTrack.textureName.Sample(t);
			if (!animationTrack.extents.keys.empty()) elementToAnimate->extents = animationTrack.extents.Sample(t);
		}

		if (ended && !animationClip.nextAnimationName.empty()) {
			float overflowSeconds = animator.currentTime - animationClip.lengthSeconds;
			SwitchToClip(animator, animationClip.nextAnimationName, overflowSeconds);
		}
	}

	/*

	NAME ENTRY UTIL

	*/

	static inline int FindCharacterIndex(const std::string& charset, char character) {
		auto position = charset.find(character);
		return (position == std::string::npos) ? 0 : (int)position;
	}

	static inline int WrapCharacter(int x, const std::string& charset) {
		int n = (int)charset.size();
		x %= n;
		if (x < 0) x += n;
		return x;
	}

	int GetLetterSlotIndexFromId(const std::string& id) {
		const char* prefix = "letterSlot_";
		if (id.rfind(prefix, 0) != 0) return -1;

		const char* num = id.c_str() + std::strlen(prefix);
		if (*num == '\0') return -1;

		int index = std::atoi(num);
		return (index >= 0 && index < 3) ? index : -1;
	}

	static void RefreshWheelSlot(UI& ui, int slotIndex) {
		const std::string& charset = ui.nameEntry.charset;
		int characterIndex = FindCharacterIndex(charset, ui.nameEntry.buffer[slotIndex]);

		char prevChar = charset[WrapCharacter(characterIndex - 1, charset)];
		char currChar = charset[WrapCharacter(characterIndex, charset)];
		char nextChar = charset[WrapCharacter(characterIndex + 1, charset)];

		if (UIElement* element = GetElementById(ui.uiElements, "letterPrev_" + std::to_string(slotIndex))) element->text = std::string(1, prevChar);
		if (UIElement* element = GetElementById(ui.uiElements, "letterCurr_" + std::to_string(slotIndex))) element->text = std::string(1, currChar);
		if (UIElement* element = GetElementById(ui.uiElements, "letterNext_" + std::to_string(slotIndex))) element->text = std::string(1, nextChar);
	}

	static void RotateCurrentLetter(UI& ui, int delta) {
		int slot = ui.nameEntry.cursor;
		const std::string& charset = ui.nameEntry.charset;
		if (charset.empty()) return;

		int charIndex = FindCharacterIndex(charset, ui.nameEntry.buffer[slot]);
		charIndex = WrapCharacter(charIndex + delta, charset);

		ui.nameEntry.buffer[slot] = charset[charIndex];
		RefreshWheelSlot(ui, slot);
	}

	static void RefreshAllSlots(UI& ui) {
		for (int i = 0; i < 3; i++) {
			RefreshWheelSlot(ui, i);
		}
	}

	

	

	/*
	
	CORE TEXT FUNCTIONS

	*/
	static inline void AddQuadUV(DRAW::UIDrawList& drawList, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, unsigned int color)
	{
		const unsigned int base = (unsigned int)drawList.vertices.size();

		drawList.vertices.push_back(DRAW::UIVertex{ {x0,y0}, {u0,v0}, color });
		drawList.vertices.push_back(DRAW::UIVertex{ {x1,y0}, {u1,v0}, color });
		drawList.vertices.push_back(DRAW::UIVertex{ {x1,y1}, {u1,v1}, color });
		drawList.vertices.push_back(DRAW::UIVertex{ {x0,y1}, {u0,v1}, color });

		drawList.indices.push_back(base + 0);
		drawList.indices.push_back(base + 1);
		drawList.indices.push_back(base + 2);
		drawList.indices.push_back(base + 0);
		drawList.indices.push_back(base + 2);
		drawList.indices.push_back(base + 3);
	}

	bool CreateFontFromTTF(entt::registry& registry, entt::entity entity, const std::string& filePath, float pixelHeight, DRAW::UIFont& outFont, int atlasWidth, int atlasHeight) {
		std::vector<unsigned char> ttfBytes;
		if (!ReadFileBytes(filePath, ttfBytes)) return false;

		int width = atlasWidth;
		int height = atlasHeight;
		int bakedWidth = width;
		int bakedHeight = height;
		std::vector<unsigned char> charAlphas;
		int bakedCount = 0;

		for (int attempt = 0; attempt < 4; attempt++) {

			charAlphas.assign(width * height, 0);

			bakedCount = stbtt_BakeFontBitmap(ttfBytes.data(), 0, pixelHeight, charAlphas.data(), width, height, UIFont::firstCharacter, UIFont::numberOfCharacters, outFont.bakedCharacters.data());

			if (bakedCount > 0) {
				bakedWidth = width;
				bakedHeight = height;
				break;
			}

			width *= 2;
			height *= 2;
			//charAlphas.resize(width * height);
		}

		if (bakedCount <= 0) return false;

		width = bakedWidth;
		height = bakedHeight;

		stbtt_fontinfo fontInfo{};

		if (!stbtt_InitFont(&fontInfo, ttfBytes.data(), stbtt_GetFontOffsetForIndex(ttfBytes.data(), 0))) return false;

		float scale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);

		int ascent, descent, lineGap;

		stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

		tinygltf::Image image{};
		image.width = width;
		image.height = height;
		image.component = 4;
		image.bits = 8;
		image.image.resize(width * height * 4);

		for (int i = 0; i < width * height; i++) {
			unsigned char alpha = charAlphas[i];
			image.image[i * 4] = 255;
			image.image[i * 4 + 1] = 255;
			image.image[i * 4 + 2] = 255;
			image.image[i * 4 + 3] = alpha;
		}

		outFont.textureId = CreateUITextureFromImage(registry, entity, image, false);
		outFont.atlasW = width;
		outFont.atlasH = height;
		outFont.pixelHeight = pixelHeight;
		outFont.lineHeight = (ascent - descent + lineGap) * scale;
		outFont.ascent = ascent * scale;
		outFont.descent = descent * scale;
		return true;
	}

	int GetOrCreateFont(entt::registry& registry, entt::entity entity, const std::string& path, int pixelSize, int atlasWidth, int atlasHeight) {
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		std::string key;
		MakeFontKey(path, pixelSize, key);
		auto fontCacheIter = vulkanUIRenderer.fontCache.find(key);
		if (fontCacheIter != vulkanUIRenderer.fontCache.end()) return fontCacheIter->second;

		UIFont font{};
		if (!CreateFontFromTTF(registry, entity, path, pixelSize, font, atlasWidth, atlasHeight)) {
			std::cout << "Failed to create font at: " << path << " with size " << pixelSize << std::endl;
			return -1;
		}

		font.filePath = path;
		font.pixelHeight = pixelSize;

		int index = vulkanUIRenderer.fonts.size();
		vulkanUIRenderer.fonts.push_back(std::move(font));
		vulkanUIRenderer.fontCache[key] = index;
		//std::cout << "Font " << key << " created with index " << index << std::endl;
		return index;
	}

	static inline void AddText(UIDrawList& drawList, UIFont& font, float2 position, Color color, Alignment alignment, const char* text) {
		TextBlockInfo textBlockInfo = CalculateTextBlockInfo(font, text);

		float topLeftX, topLeftY;
		switch (alignment.horizontal) {
		default:
		case HorizontalAlignment::LEFT:
			topLeftX = position.x;
			break;
		case HorizontalAlignment::CENTER:
			topLeftX = position.x - textBlockInfo.textBlockWidth * 0.5f;
			break;
		case HorizontalAlignment::RIGHT:
			topLeftX = position.x - textBlockInfo.textBlockWidth;
			break;
		}
		switch (alignment.vertical) {
		default:
		case VerticalAlignment::TOP:
			topLeftY = position.y;
			break;
		case VerticalAlignment::MIDDLE:
			topLeftY = position.y - textBlockInfo.textBlockHeight * 0.5f;
			break;
		case VerticalAlignment::BOTTOM:
			topLeftY = position.y - textBlockInfo.textBlockHeight;
		}

		float horizontalFactor = AlignFactor(alignment.horizontal);

		unsigned int combinedColor = PackRGBA8(color.r, color.g, color.b, color.a);

		int lineIndex = 0;
		float xPosition = topLeftX + (textBlockInfo.textBlockWidth - textBlockInfo.lineWidths[lineIndex]) * horizontalFactor;
		float yPosition = topLeftY + font.ascent;

		for (const char* currentChar = text; *currentChar; ++currentChar) {
			char character = *currentChar;
			if (character == '\r') continue;
			if (character == '\n') {
				lineIndex++;
				xPosition = topLeftX + (textBlockInfo.textBlockWidth - textBlockInfo.lineWidths[lineIndex]) * horizontalFactor;
				yPosition += font.lineHeight;
				continue;
			}

			if (character < font.firstCharacter || character >= font.firstCharacter + font.numberOfCharacters) continue;

			stbtt_aligned_quad quad{};

			stbtt_GetBakedQuad(font.bakedCharacters.data(), font.atlasW, font.atlasH, character - font.firstCharacter, &xPosition, &yPosition, &quad, 1);

			AddQuadUV(drawList, quad.x0, quad.y0, quad.x1, quad.y1, quad.s0, quad.t0, quad.s1, quad.t1, combinedColor);
		}
	}

	/*
	CORE QUAD FUNCTIONS
	*/

	static inline void AddQuad(UIDrawList& drawList, Quad& quad) {
		const unsigned int base = (unsigned int)drawList.vertices.size();

		unsigned int color = PackRGBA8(quad.color.r, quad.color.g, quad.color.b, quad.color.a);
		drawList.vertices.push_back(UIVertex{ {quad.topLeft.x,quad.topLeft.y}, {0,0}, color });
		drawList.vertices.push_back(UIVertex{ {quad.bottomRight.x, quad.topLeft.y}, {1, 0}, color });
		drawList.vertices.push_back(UIVertex{ {quad.bottomRight.x, quad.bottomRight.y}, {1, 1}, color });
		drawList.vertices.push_back(UIVertex{ {quad.topLeft.x, quad.bottomRight.y}, {0, 1}, color });

		drawList.indices.push_back(base + 0);
		drawList.indices.push_back(base + 1);
		drawList.indices.push_back(base + 2);
		drawList.indices.push_back(base + 0);
		drawList.indices.push_back(base + 2);
		drawList.indices.push_back(base + 3);
	}

	

	/*
	
	ACTUAL FUNCTIONS TO CALL TO RENDER QUADS AND TEXT
	
	*/

	void RenderQuad(entt::registry& registry, entt::entity entity, UIDrawList& uiDrawList, 
						  std::string textureName, Quad& quad,
						  unsigned int windowWidth, unsigned int windowHeight)
	{
		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		unsigned int preOperationSize = uiDrawList.indices.size();

		AddQuad(uiDrawList, quad);

		UIDrawCmd drawCmd{};

		drawCmd.clip.offset = { 0,0 };
		drawCmd.clip.extent = { windowWidth, windowHeight };
		drawCmd.firstIndex = preOperationSize;
		drawCmd.indexCount = (unsigned int)uiDrawList.indices.size() - preOperationSize;
		drawCmd.vertexOffset = 0;
		auto textureCacheIter = vulkanUIRenderer.textureCache.find(textureName);
		if (textureCacheIter == vulkanUIRenderer.textureCache.end()) {
			drawCmd.textureId = 0;
		}
		else {
			drawCmd.textureId = textureCacheIter->second;
		}
		
		uiDrawList.drawCommands.push_back(drawCmd);
	}

	void RenderText(entt::registry& registry, entt::entity entity, UIDrawList& uiDrawList,
		std::string fontName, Text& text,
		unsigned int windowWidth, unsigned int windowHeight, float uiScale) {

		auto& vulkanUIRenderer = registry.get<VulkanUIRenderer>(entity);

		std::string scaledKey = MakeScaledFontKey(fontName, uiScale);

		if (vulkanUIRenderer.fontCache.find(scaledKey) == vulkanUIRenderer.fontCache.end()) {
			std::string path;
			int size = 0;
			if (ParseFontName(scaledKey, path, size)) {
				GetOrCreateFont(registry, entity, "..\\Fonts\\" + path + ".ttf", size);
			}
		}

		unsigned int preOperationSize = uiDrawList.indices.size();

		AddText(uiDrawList, vulkanUIRenderer.fonts[vulkanUIRenderer.fontCache[scaledKey]], text.pos, text.color, text.alignment, text.text.c_str());

		UIDrawCmd drawCmd{};

		drawCmd.clip.offset = { 0,0 };
		drawCmd.clip.extent = { windowWidth, windowHeight };
		drawCmd.firstIndex = preOperationSize;
		drawCmd.indexCount = (unsigned int)uiDrawList.indices.size() - preOperationSize;
		drawCmd.vertexOffset = 0;
		drawCmd.textureId = vulkanUIRenderer.fonts[vulkanUIRenderer.fontCache[scaledKey]].textureId;

		uiDrawList.drawCommands.push_back(drawCmd);
	}

	/*
	
	UI ANIMATION BUILD FUNCTIONS
	
	*/

	static void CreateStartGameUIAnimation(UIAnimator& animator) {
		AnimationClip clip{};

		clip.lengthSeconds = 4.0f;
		clip.loop = false;

		ElementAnimationTrack logoAnimation{};

		logoAnimation.id = "logo";
		logoAnimation.color.keys = {
			{0.5f, {0,0,0,0}},
			{3.0f, {245,245,255,255}}
		};

		logoAnimation.pos.keys = {
			{0.5f, BuildRectTransform(0.5f, 0.5f, 0, 50)},
			{3.0f, BuildRectTransform(0.5f, 0.5f, 0, -150)}
		};

		ElementAnimationTrack textAnimation{};
		textAnimation.id = "startTextFrame";

		textAnimation.pos.keys = {
			{3.0f, BuildRectTransform(0.5f, 1.0f, -125.0f, 200.0f)},
			{4.0f, BuildRectTransform(0.5f, 1.0f, -125.0f, -50.0f)}
		};

		textAnimation.color.keys = {
			{3.0f, {155,155,155,0}},
			{4.0f, {155,155,155,255}}
		};

		ElementAnimationTrack textAnimation2{};
		textAnimation2.id = "quitTextFrame";

		textAnimation2.pos.keys = {
			{3.0f, BuildRectTransform(0.5f, 1.0f, 125.0f, 250.0f)},
			{4.0f, BuildRectTransform(0.5f, 1.0f, 125.0f, -50.0f)}
		};

		textAnimation2.color.keys = {
			{3.0f, {155,155,155,0}},
			{4.0f, {155,155,155,255}}
		};

		clip.animationTracks.push_back(std::move(textAnimation));
		clip.animationTracks.push_back(std::move(textAnimation2));
		clip.animationTracks.push_back(std::move(logoAnimation));

		UIAnimationEvent playMusic{};
		playMusic.eventType = UIAnimationEventType::RUN_COMMAND;
		playMusic.commandToRun = UICommand::SET_MUSIC;
		playMusic.data = "menu";

		clip.events.push_back({ 0.25f, playMusic });

		UIAnimationEvent enableSelection{};
		enableSelection.eventType = UIAnimationEventType::ENABLE_SELECTION;

		clip.events.push_back({ clip.lengthSeconds, enableSelection });

		int x;
		int y;
		int startOffsetX;
		for (int i = 0; i < 10; i++) {
			ElementAnimationTrack track{};
			track.id = "highScore_" + std::to_string(i);
			x = i < 5 ? -175 : 175;
			startOffsetX = i < 5 ? -600 : 600;
			y = 50 + (i % 5) * 50;
			track.pos.keys = {
				{3.0f, BuildRectTransform(0.5f, 0.5f, x + startOffsetX, y)},
				{4.0f, BuildRectTransform(0.5f, 0.5f, x, y)}
			};
			clip.animationTracks.push_back(track);
		}

		clip.nextAnimationName = "startGameIdle";

		animator.animations.insert({ "startGame", clip });
	}

	static void CreateStartGameUIIdleAnimation(UIAnimator& animator) {
		AnimationClip clip{};

		clip.lengthSeconds = 15.0f;
		clip.loop = true;

		ElementAnimationTrack logoAnimation{};
		logoAnimation.id = "logo";
		logoAnimation.color.keys = {
			{0.0f, {245,245,255,255}},
			{3.75f, {255,255,255,255}},
			{7.5f, {245,245,245,245}},
			{11.25f, {255,255,255,255}},
			{15.0f, {245,245,255,255}}
		};
		logoAnimation.extents.keys = {
			{0.0f, {360,160}},
			{7.5f, {396, 176}},
			{15.0f, {360,160}}
		};

		ElementAnimationTrack backgroundAnimation{};
		backgroundAnimation.id = "background";
		backgroundAnimation.color.keys = {
			{0.0f, {10,10,30,255}},
			{3.75f, {30,30,100,255}},
			{7.5, {10,10,30,255}},
			{11.25f, {30,30,100,255}},
			{15.0f, {10,10,30,255}}
		};

		clip.animationTracks.push_back(logoAnimation);
		clip.animationTracks.push_back(backgroundAnimation);

		animator.animations.insert({ "startGameIdle", clip });
	}

	static void CreateNameEntryAnimations(UIAnimator& animator) {
		AnimationClip clip{};

		clip.lengthSeconds = 1;
		clip.loop = false;

		UIAnimationEvent enableSelection{};
		enableSelection.eventType = UIAnimationEventType::ENABLE_SELECTION;

		clip.events.push_back({ 0.01f, enableSelection });

		animator.animations.insert({ "nameEntry", clip });
	}

	static void CreateDeathUIAnimations(UIAnimator& animator) {
		AnimationClip clip0{};
		AnimationClip clip1{};
		AnimationClip clip2{};
		AnimationClip clip3{};

		clip0.lengthSeconds = 2;
		clip0.loop = false;
		clip1.lengthSeconds = 2;
		clip1.loop = false;
		clip2.lengthSeconds = 2;
		clip2.loop = false;
		clip3.lengthSeconds = 2;
		clip3.loop = false;

		ElementAnimationTrack life0{};
		ElementAnimationTrack life1{};
		ElementAnimationTrack life2{};
		ElementAnimationTrack life3{};
		life0.id = "life_0";
		life1.id = "life_1";
		life2.id = "life_2";
		life3.id = "life_3";

		life0.color.keys = {
			{0.0f, {255,255,255,255}},
			{0.5f, {255,255,255,255}},
			{1.0f, {255,255,255,0}}
		};
		life0.textureName.keys = {
			{0.5f, "lives_icon"},
			{0.51f, "lives_icon_lost"}
		};
		life1.color.keys = {
			{0.0f, {255,255,255,255}},
			{0.5f, {255,255,255,255}},
			{1.0f, {255,255,255,0}}
		};
		life1.textureName.keys = {
			{0.5f, "lives_icon"},
			{0.51f, "lives_icon_lost"}
		};
		life2.color.keys = {
			{0.0f, {255,255,255,255}},
			{0.5f, {255,255,255,255}},
			{1.0f, {255,255,255,0}}
		};
		life2.textureName.keys = {
			{0.5f, "lives_icon"},
			{0.51f, "lives_icon_lost"}
		};
		life3.color.keys = {
			{0.0f, {255,255,255,255}},
			{0.5f, {255,255,255,255}},
			{1.0f, {255,255,255,0}}
		};
		life3.textureName.keys = {
			{0.5f, "lives_icon"},
			{0.51f, "lives_icon_lost"}
		};

		clip0.animationTracks.push_back(std::move(life0));
		clip1.animationTracks.push_back(std::move(life1));
		clip2.animationTracks.push_back(std::move(life2));
		clip3.animationTracks.push_back(std::move(life3));

		ElementAnimationTrack continueText{};
		continueText.id = "spaceToContinueText";
		continueText.color.keys = { 
			{0.999f, {0,0,0,255}},
			{1.0f, {50,150,255,255}}
		};

		//clip1.animationTracks.push_back(continueText);
		//clip2.animationTracks.push_back(continueText);

		UIAnimationEvent sound{};
		sound.eventType = UIAnimationEventType::PLAY_SOUND;
		sound.data = "loseLife";

		clip0.events.push_back({ 0.5f, sound });
		clip1.events.push_back({ 0.5f, sound });
		clip2.events.push_back({ 0.5f, sound });
		clip3.events.push_back({ 0.5f, sound });

		UIAnimationEvent transitionToTreasureScreen{};
		transitionToTreasureScreen.eventType = UIAnimationEventType::SET_UI_STATE;
		transitionToTreasureScreen.newState = UIState::PRE_IN_GAME_RESPAWN;

		clip1.events.push_back({ clip1.lengthSeconds, transitionToTreasureScreen });
		clip2.events.push_back({ clip2.lengthSeconds, transitionToTreasureScreen });
		clip3.events.push_back({ clip3.lengthSeconds, transitionToTreasureScreen });

		UIAnimationEvent transitionToGameOver{};
		transitionToGameOver.eventType = UIAnimationEventType::SET_UI_STATE;
		transitionToGameOver.newState = UIState::GAME_OVER;

		clip0.events.push_back({ clip0.lengthSeconds, transitionToGameOver });

		animator.animations.insert({ "deathScreen_0", clip0 });
		animator.animations.insert({ "deathScreen_1", clip1 });
		animator.animations.insert({ "deathScreen_2", clip2 });
		animator.animations.insert({ "deathScreen_3", clip3 });
	}

	static void CreatePreInGameUIAnimation(UIAnimator& animator) {
		AnimationClip clip{};
		clip.lengthSeconds = 4.0f;

		for (int i = 0; i < 6; i++) {
			ElementAnimationTrack frameTrack{};
			ElementAnimationTrack iconTrack{};
			iconTrack.id = "treasureIcon" + std::to_string(i + 1);
			frameTrack.id = "treasureBackground" + std::to_string(i + 1);
			frameTrack.color.keys = {
				{(i+1) * 0.25f - 0.01f, {25,100,240,0}},
				{(i + 1) * 0.25f, {25,100,240,210}}
			};
			iconTrack.color.keys = {
				{(i+1) * 0.25f - 0.01f, {255,255,255,0}},
				{(i + 1) * 0.25f, {255,255,255,255}}
			};

			clip.animationTracks.push_back(std::move(frameTrack));
			clip.animationTracks.push_back(std::move(iconTrack));

			UIAnimationEvent sound{};
			sound.eventType = UIAnimationEventType::PLAY_SOUND;
			sound.data = "menuAppear";

			clip.events.push_back({ (i + 1) * 0.25f, sound});
		}

		UIAnimationEvent disableSelection{};
		disableSelection.eventType = UIAnimationEventType::DISABLE_SELECTION;

		clip.events.push_back({ 0.01f, disableSelection });

		UIAnimationEvent stopMusic{};
		stopMusic.eventType = UIAnimationEventType::RUN_COMMAND;
		stopMusic.commandToRun = UICommand::STOP_MUSIC;

		clip.events.push_back({ 0.01f, stopMusic });

		UIAnimationEvent transitionToInGame{};
		transitionToInGame.eventType = UIAnimationEventType::RUN_COMMAND;
		transitionToInGame.commandToRun = UICommand::START_GAME;

		clip.events.push_back({ clip.lengthSeconds, transitionToInGame });

		animator.animations.insert({ "treasureScreen", clip });

		

		clip.events.clear();

		for (int i = 0; i < 6; i++) {

			UIAnimationEvent sound{};
			sound.eventType = UIAnimationEventType::PLAY_SOUND;
			sound.data = "menuAppear";

			clip.events.push_back({ (i + 1) * 0.25f, sound });
		}

		UIAnimationEvent respawn{};
		respawn.eventType = UIAnimationEventType::RUN_COMMAND;
		respawn.commandToRun = UICommand::RESPAWN;

		clip.events.push_back({ clip.lengthSeconds, respawn });

		animator.animations.insert({ "treasureScreen_Respawn", clip });
	}

	static void CreateInGameUIAnimations(UIAnimator& animator) {
		AnimationClip clip{};
		clip.lengthSeconds = 4.0f;

		ElementAnimationTrack bottomBarTrack{};
		bottomBarTrack.id = "bottomBar";
		bottomBarTrack.pos.keys = {
			{0.0f, BuildRectTransform(0.5f, 1, 0, 200)},
			{1.5f, BuildRectTransform(0.5f, 1, 0, 0)}
		};

		clip.animationTracks.push_back(bottomBarTrack);

		ElementAnimationTrack topBarTrack{};
		topBarTrack.id = "topBar";
		topBarTrack.pos.keys = {
			{0.0f, BuildRectTransform(0.5f, 0, 0, -200)},
			{1.5f, BuildRectTransform(0.5f, 0, 0, 0)}
		};

		clip.animationTracks.push_back(topBarTrack);

		ElementAnimationTrack readyTextTrack{};
		readyTextTrack.id = "readyText";
		readyTextTrack.color.keys = {
			{2.99f, {255,255,255,255}},
			{3.0f, {255,255,255,0}}
		};

		clip.animationTracks.push_back(readyTextTrack);

		ElementAnimationTrack goTextTrack{};
		goTextTrack.id = "goText";
		goTextTrack.color.keys = {
			{2.99f, {255,255,255,0}},
			{3.0f, {255,255,255,255}},
			{3.99f, {255,255,255,255}},
			{4.0f, {255,255,255,0}}
		};

		clip.animationTracks.push_back(goTextTrack);

		ElementAnimationTrack frameTrack{};
		frameTrack.id = "frame";
		frameTrack.color.keys = {
			{0.99f, {50,255,50,255}},
			{1.0f, {50,200,200,255}},
			{1.99f, {50,200,200,255}},
			{2.0f, {200,50,50,255}},
			{2.99f, {200,50,50,255}},
			{3.0f, {255,255,255,255}},
			{4.0f, {255,255,255,0}}
		};

		clip.animationTracks.push_back(frameTrack);

		animator.animations.insert({ "inGameStartup", clip });
	}

	static void CreateGameOverUIAnimations(UIAnimator& animator) {
		AnimationClip gameOver{};
		AnimationClip gameOverNewHighScore{};
		
		gameOver.lengthSeconds = 3.0f;
		gameOverNewHighScore.lengthSeconds = 5.0f;

		ElementAnimationTrack newHighScoreTextTrack{};
		newHighScoreTextTrack.id = "newHighScoreText";
		newHighScoreTextTrack.color.keys = {
			{0.99f, {50,255,50,255}},
			{1.0f, {50,200,200,255}},
			{1.99f, {50,200,200,255}},
			{2.0f, {200,50,50,255}},
			{2.99f, {200,50,50,255}},
			{3.0f, {255,255,255,255}},
			{4.0f, {255,255,255,0}}
		};

		gameOverNewHighScore.animationTracks.push_back(std::move(newHighScoreTextTrack));

		UIAnimationEvent gameOverEvent{};
		gameOverEvent.eventType = UIAnimationEventType::SET_UI_STATE;
		gameOverEvent.newState = UIState::START_GAME;

		UIAnimationEvent gameOverNewHighscoreEvent{};
		gameOverNewHighscoreEvent.eventType = UIAnimationEventType::SET_UI_STATE;
		gameOverNewHighscoreEvent.newState = HIGH_SCORE_ENTRY;

		gameOver.events.push_back({ gameOver.lengthSeconds, gameOverEvent });
		gameOverNewHighScore.events.push_back({ gameOverNewHighScore.lengthSeconds, gameOverNewHighscoreEvent });

		animator.animations.insert({ "gameOver" , gameOver });
		animator.animations.insert({ "gameOver_newHighScore", gameOverNewHighScore });
	}

	static void CreateWinUIAnimations(UIAnimator& animator) {
		AnimationClip winScreen{};
		AnimationClip winScreen_newHighScore{};

		winScreen.lengthSeconds = 3.0f;
		winScreen_newHighScore.lengthSeconds = 5.0f;

		ElementAnimationTrack newHighScoreTextTrack{};
		newHighScoreTextTrack.id = "newHighScoreText";
		newHighScoreTextTrack.color.keys = {
			{0.99f, {50,255,50,255}},
			{1.0f, {50,200,200,255}},
			{1.99f, {50,200,200,255}},
			{2.0f, {200,50,50,255}},
			{2.99f, {200,50,50,255}},
			{3.0f, {255,255,255,255}},
			{4.0f, {255,255,255,0}}
		};

		winScreen_newHighScore.animationTracks.push_back(std::move(newHighScoreTextTrack));

		UIAnimationEvent gameOverEvent{};
		gameOverEvent.eventType = UIAnimationEventType::SET_UI_STATE;
		gameOverEvent.newState = UIState::START_GAME;

		UIAnimationEvent gameOverNewHighscoreEvent{};
		gameOverNewHighscoreEvent.eventType = UIAnimationEventType::SET_UI_STATE;
		gameOverNewHighscoreEvent.newState = HIGH_SCORE_ENTRY;

		winScreen.events.push_back({ winScreen.lengthSeconds, gameOverEvent });
		winScreen_newHighScore.events.push_back({ winScreen_newHighScore.lengthSeconds, gameOverNewHighscoreEvent });

		animator.animations.insert({ "winScreen" , winScreen });
		animator.animations.insert({ "winScreen_newHighScore", winScreen_newHighScore });
	}

	static void CreateTimeUpAnimation(UIAnimator& uiAnimator) {
		AnimationClip clip{};
		clip.lengthSeconds = 3.0f;

		ElementAnimationTrack backgroundTrack;
		backgroundTrack.id = "background";
		backgroundTrack.color.keys = {
			{0.0f, {0,0,0,0}},
			{1.0f, {10,10,30,255}}
		};

		ElementAnimationTrack timeUpTextTrack;
		timeUpTextTrack.id = "timeUpText";
		timeUpTextTrack.color.keys = {
			{0.5f, {0,0,0,0}},
			{1.5f, {255,255,255,255}}
		};
		timeUpTextTrack.pos.keys = {
			{0.5f, BuildRectTransform(0.5f, 0.5f, 0, -200)},
			{1.5f, BuildRectTransform(0.5f, 0.5f, 0, 0)}
		};

		clip.animationTracks.push_back(backgroundTrack);
		clip.animationTracks.push_back(timeUpTextTrack);

		UIAnimationEvent event;
		event.eventType = UIAnimationEventType::SET_UI_STATE;
		event.newState = UIState::GAME_OVER;

		clip.events.push_back({ clip.lengthSeconds, event });

		uiAnimator.animations.insert({ "timeUp", clip });
	}

	static void CreateCompletedAnimation(UIAnimator& animator) {
		AnimationClip clip{};

		clip.lengthSeconds = 4.5f;
		ElementAnimationTrack backgroundTrack;
		backgroundTrack.id = "background";
		backgroundTrack.color.keys = {
			{0.0f, {0,0,0,0}},
			{1.0f, {10,10,30,255}}
		};

		clip.animationTracks.push_back(backgroundTrack);

		ElementAnimationTrack allPartsCollectedTrack;
		allPartsCollectedTrack.id = "completedText";
		allPartsCollectedTrack.color.keys = {
			{0.5f, {0,0,0,0}},
			{1.5f, {255,255,255,255}}
		};
		allPartsCollectedTrack.pos.keys = {
			{0.5f, BuildRectTransform(0.5f, 0.5f, 0, 100)},
			{1.5f, BuildRectTransform(0.5f, 0.5f, 0, 0)}
		};

		clip.animationTracks.push_back(allPartsCollectedTrack);

		ElementAnimationTrack timeRemainingLabel;
		timeRemainingLabel.id = "timeRemainingLabel";
		timeRemainingLabel.color.keys = {
			{2.49f, {0,0,0,0}},
			{2.5f, {200,200,255,255}}
		};

		clip.animationTracks.push_back(timeRemainingLabel);

		ElementAnimationTrack timeRemainingText;
		timeRemainingText.id = "timeRemainingText";
		timeRemainingText.color.keys = {
			{2.74f, {0,0,0,0}},
			{2.75f, {255,255,255,255}}
		};

		clip.animationTracks.push_back(timeRemainingText);

		ElementAnimationTrack scoreBonusLabel;
		scoreBonusLabel.id = "scoreBonusLabel";
		scoreBonusLabel.color.keys = {
			{3.24f, {0,0,0,0}},
			{3.25f, {200,200,255,255}}
		};

		clip.animationTracks.push_back(scoreBonusLabel);

		ElementAnimationTrack scoreBonusText;
		scoreBonusText.id = "scoreBonusText";
		scoreBonusText.color.keys = {
			{3.49f, {0,0,0,0}},
			{3.5f, {255,255,255,255}}
		};

		clip.animationTracks.push_back(scoreBonusText);

		ElementAnimationTrack finalScoreLabel;
		finalScoreLabel.id = "finalScoreLabel";
		finalScoreLabel.color.keys = {
			{3.99f, {0,0,0,0}},
			{4.0f, {200,200,255,255}}
		};

		clip.animationTracks.push_back(finalScoreLabel);

		ElementAnimationTrack finalScoreText;
		finalScoreText.id = "finalScoreText";
		finalScoreText.color.keys = {
			{4.24f, {0,0,0,0}},
			{4.25f, {255,255,255,255}}
		};

		clip.animationTracks.push_back(finalScoreText);

		animator.animations.insert({ "completed", clip });
	}

	/*
	
	UI ELEMENT CREATION HELPER FUNCTIONS
	
	*/

	static void BuildQuadElement(UI& ui, const char* id, RectTransform transform, float2 extents, Alignment alignment, Color color, const char* textureName, const char* parentId = "", uint8_t scaleFlags = UIElementScaleFlags::SCALE_ALL) {
		UIElement element{};
		element.id = id;
		element.type = UIElementType::QUAD;
		element.localTransform = transform;
		element.extents = extents;
		element.alignment = alignment;
		element.color = color;
		element.textureName = textureName;
		element.parentId = parentId;
		element.scaleFlags = scaleFlags;
		ui.uiElements.push_back(element);
	}

	static void BuildTextElement(UI& ui, const char* id, RectTransform transform, Alignment alignment, Color color, const char* fontName, const char* text, const char* parentId = "", uint8_t scaleFlags = UIElementScaleFlags::SCALE_ALL) {
		UIElement element{};
		element.id = id;
		element.type = UIElementType::TEXT;
		element.localTransform = transform;
		element.alignment = alignment;
		element.color = color;
		element.fontName = fontName;
		element.text = text;
		element.parentId = parentId;
		element.scaleFlags = scaleFlags;
		ui.uiElements.push_back(element);
	}

	static void BuildSelectableQuadElement(UI& ui, const char* id, RectTransform transform, float2 extents, Alignment alignment, Color color, const char* textureName, const char* parentId = "", uint8_t scaleFlags = UIElementScaleFlags::SCALE_ALL, UICommand command = UICommand::NONE, std::string commandContext = "", std::string up = "", std::string down = "", std::string right = "", std::string left = "") {
		UIElement element{};
		element.id = id;
		element.type = UIElementType::QUAD;
		element.localTransform = transform;
		element.extents = extents;
		element.alignment = alignment;
		element.color = color;
		element.textureName = textureName;
		element.parentId = parentId;
		element.scaleFlags = scaleFlags;
		ui.uiElements.push_back(element);

		UISelectable selectable{};
		selectable.uiElementId = id;
		selectable.command = command;
		selectable.commandContext = commandContext;
		selectable.upId = up;
		selectable.downId = down;
		selectable.rightId = right;
		selectable.leftId = left;
		ui.uiSelectables.push_back(selectable);
	}

	static void BuildSelectableTextElement(UI& ui, const char* id, RectTransform transform, Alignment alignment, Color color, const char* fontName, const char* text, const char* parentId = "", uint8_t scaleFlags = UIElementScaleFlags::SCALE_ALL, UICommand command = UICommand::NONE, std::string commandContext = "", std::string up = "", std::string down = "", std::string right = "", std::string left = "") {
		UIElement element{};
		element.id = id;
		element.type = UIElementType::TEXT;
		element.localTransform = transform;
		element.alignment = alignment;
		element.color = color;
		element.fontName = fontName;
		element.text = text;
		element.parentId = parentId;
		element.scaleFlags = scaleFlags;
		ui.uiElements.push_back(element);

		UISelectable selectable{};
		selectable.uiElementId = id;
		selectable.command = command;
		selectable.commandContext = commandContext;
		selectable.upId = up;
		selectable.downId = down;
		selectable.rightId = right;
		selectable.leftId = left;
		ui.uiSelectables.push_back(selectable);
	}

	/*
	
	UI STATE BUILD FUNCTIONS
	
	*/

	static void BuildStartGameUI(UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		ui.uiElements.clear();
		ui.uiSelectables.clear();

		float2 center = { windowWidth * 0.5f, windowHeight * 0.5f };
		
		BuildQuadElement(ui, "background", BuildRectTransform(0.5f,0.5f,0,0), {windowWidth, windowHeight}, Align::CenterMiddle, {10,10,30,255}, "background", "", 0);
		
		BuildQuadElement(ui, "logo", BuildRectTransform(0.5f, 0.5f,0,-130), {360,160}, Align::CenterMiddle, {255,255,255,255}, "game_logo");
		
		BuildSelectableQuadElement(ui, "startTextFrame", BuildRectTransform(0.5f, 1.0f, -125.0f, -50.0f), {200,50}, Align::CenterMiddle, {50,50,225,255}, "frame", "", UIElementScaleFlags::SCALE_ALL, UICommand::CHANGE_GAME_STATE, "preInGame", "", "", "quitTextFrame", "quitTextFrame");

		BuildSelectableQuadElement(ui, "quitTextFrame", BuildRectTransform(0.5f, 1.0f, 125.0f, -50.0f), {200, 50}, Align::CenterMiddle, {50,50,225,255}, "frame", "", UIElementScaleFlags::SCALE_ALL, UICommand::QUIT, "", "", "", "startTextFrame", "startTextFrame");

		BuildTextElement(ui, "text", BuildRectTransform(0.5f, 0.5f,0,0), Align::CenterMiddle, {255,255,255,255}, "EuropeanTeletext|30", "START", "startTextFrame");

		BuildTextElement(ui, "text2", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "EuropeanTeletext|30", "QUIT", "quitTextFrame");

		std::vector<GAME::ScoreEntry> scores = GAME::SaveScore::Load();

		int x = 0;
		int y = 50;
		for (int i = 0; i < 10; i++) {
			std::string id = "highScore_" + std::to_string(i);
			x = i < 5 ? -175 : 175;
			y = 50 + (i % 5) * 50;
			Color color = { 160,160,160,255 };
			if (i == 0) {
				color = { 240,219,31,255 };
			}
			else if (i == 1) {
				color = { 235,235,235,255 };
			}
			else if (i == 2) {
				color = { 184,130,31,255 };
			}
			std::string numString = i < 9 ? std::to_string(i + 1) + "   " : std::to_string(i + 1) + "  ";
			std::string text = numString + scores[i].name + "   " + FormatScore(scores[i].score, 7);
			BuildTextElement(ui, id.c_str(), BuildRectTransform(0.5f, 0.5f, x, y), Align::CenterMiddle, color, "EuropeanTeletext|18", text.c_str());
		}

		if (playAnimation) {
			ui.uiAnimator.currentTime = 0.0f;
			ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["startGame"];
			ui.uiSelector.currentlyFocusedId = "startTextFrame";
		}
		
	}

	

	static void BuildPreInGameUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight,  bool respawn = false, bool playAnimation = true) {
		ui.uiElements.clear();
		ui.uiSelectables.clear();

		float2 center = { windowWidth * 0.5f, windowHeight * 0.5f };

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 10,10,30,255 }, "background", "", 0);

		BuildQuadElement(ui, "titleFrame", BuildRectTransform(0.5f, 0.5f, 0, -225), { 600, 150 }, Align::CenterBottom, { 0,50,230,255 }, "frame");

		BuildTextElement(ui, "titleText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "EuropeanTeletext|36", "COLLECTED PARTS", "titleFrame");

		BuildQuadElement(ui, "partsBackground", BuildRectTransform(0.5f, 0.5f, 0, 150), { 480, 320 }, Align::CenterMiddle, { 0,50,230,155 }, "frame");

		auto& levelManager = registry.get<GAME::LevelManager>(*registry.view<GAME::LevelManager>().begin());
		
		for (int i = 0; i < 6; i++) {
			std::string backgroundId = "treasureBackground" + std::to_string(i + 1);
			std::string iconId = "treasureIcon" + std::to_string(i + 1);
			float xOffset = (i % 3) * 130 + 110;
			float yOffset = i < 3 ? 110 : 210;
			BuildQuadElement(ui, backgroundId.c_str(), BuildRectTransform(0, 0, xOffset, yOffset), { 80,80 }, Align::CenterMiddle, { 25,100,240,210 }, "frame", "partsBackground");

			if (HasTreasure(levelManager.levelAndTreasureId, i+1)) {
				BuildQuadElement(ui, iconId.c_str(), BuildRectTransform(0.5f, 0.5f, 0, 0), { 64,64 }, Align::CenterMiddle, { 255,255,255,255 }, iconId.c_str(), backgroundId.c_str());
			}
			else {
				BuildQuadElement(ui, iconId.c_str(), BuildRectTransform(0.5f, 0.5f, 0, 0), { 64,64 }, Align::CenterMiddle, { 255,255,255,255 }, "uncollected_treasure", backgroundId.c_str());

			}
		}

		if (playAnimation) {
			ui.uiAnimator.currentTime = 0.0f;
			if (respawn) {
				ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["treasureScreen_Respawn"];
			}
			else {
				ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["treasureScreen"];
			}
		}

		
		
	}

	static void BuildDeathUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		auto& gameManager = registry.view<GAME::GameManager>();
		GAME::Lives lives = registry.get<GAME::Lives>(*(gameManager.begin()));

		ui.uiElements.clear();
		ui.uiSelectables.clear();

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 10,10,30,255 }, "background", "", 0);

		BuildTextElement(ui, "livesRemainingText", BuildRectTransform(0.5f, 0.5f, -20, 0), Align::RightMiddle, { 255,255,255,255 }, "MidnightLetters|36", "LIVES");

		for (int i = 0; i < lives.value+1; i++) {
			std::string id = "life_" + std::to_string(i);
			BuildQuadElement(ui, id.c_str(), BuildRectTransform(0.5f, 0.5f, 80 * i, 0), { 64,64 }, Align::LeftMiddle, { 255,255,255,255 }, "lives_icon");
		}

		BuildTextElement(ui, "spaceToContinueText", BuildRectTransform(0.5f, 0.5f, 0, 75.0f), Align::CenterTop, { 50,100,255,0 }, "EuropeanTeletext|18", "PRESS SPACE TO CONTINUE");

		if (playAnimation) {
			ui.uiAnimator.currentTime = 0.0f;
			std::string animationName = "deathScreen_" + std::to_string(lives.value);
			ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations[animationName];
		}

		
	}

	static void BuildWinUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		auto& gameManager = registry.view<GAME::GameManager>();
		GAME::Score score = registry.get<GAME::Score>(*(gameManager.begin()));

		ui.uiElements.clear();
		ui.uiSelectables.clear();

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 10,10,30,255 }, "background", "", 0);

		BuildTextElement(ui, "winText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|36", "YOU ARE BECOME WINNER");

		BuildTextElement(ui, "newHighScoreText", BuildRectTransform(0.5f, 0.5f, 0, 100), Align::CenterTop, { 50,50,225,0 }, "EuropeanTeletext|30", "NEW HIGH SCORE!!!");

		if (!playAnimation) return;

		ui.uiAnimator.currentTime = 0.0f;
		if (GAME::SaveScore::IsTopTenScore(score.value)) {
			ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["winScreen_newHighScore"];
		}
		else {
			ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["winScreen"];
		}
	}

	static void BuildTimeUpUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		
		ui.uiElements.clear();
		ui.uiSelectables.clear();

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 10,10,30,255 }, "background", "", 0);
		
		BuildTextElement(ui, "timeUpText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|36", "TIME'S UP!");

		if (!playAnimation) return;

		ui.uiAnimator.currentTime = 0.0f;
		ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["timeUp"];
	}

	static void BuildInGameUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight, bool playAnimation) {
		auto& gameManager = registry.view<GAME::GameManager>();
		GAME::Score score = registry.get<GAME::Score>(*(gameManager.begin()));
		GAME::Lives lives = registry.get<GAME::Lives>(*(gameManager.begin()));

		ui.uiElements.clear();
		ui.uiSelectables.clear();

		BuildQuadElement(ui, "frame", BuildRectTransform(0.5f, 0.5f, 0, 0), { 450,200 }, Align::CenterMiddle, { 0,0,155,255 }, "frame", "");

		BuildTextElement(ui, "readyText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|36", "READY...", "frame");

		BuildTextElement(ui, "goText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|36", "GO!!!", "frame");

		BuildQuadElement(ui, "bottomBar", BuildRectTransform(0.5f, 1.0f, 0, 50), { windowWidth, 50 }, Align::CenterBottom, { 0,0,0,225 }, "default", "", UIElementScaleFlags::SCALE_OFFSET | UIElementScaleFlags::SCALE_EXTENTS_Y);

		BuildQuadElement(ui, "topBar", BuildRectTransform(0.5f, 0.0f, 0, 50), { windowWidth, 50 }, Align::CenterTop, { 0,0,0,225 }, "default", "", UIElementScaleFlags::SCALE_OFFSET | UIElementScaleFlags::SCALE_EXTENTS_Y);

		std::string scoreText = "SCORE " + std::to_string(score.value);

		BuildTextElement(ui, "scoreText", BuildRectTransform(0.0f, 0.5f, 75, 0), Align::LeftMiddle, { 255,255,255,255 }, "EuropeanTeletext|18", scoreText.c_str(), "bottomBar");

		BuildQuadElement(ui, "livesContainer", BuildRectTransform(1.0f, 0.5f, -75, 0), { 75, 50 }, Align::RightMiddle, { 0,0,0,0 }, "default", "bottomBar");
		
		BuildQuadElement(ui, "livesIcon", BuildRectTransform(0.0f, 0.5f, 0, 0), { 32,32 }, Align::LeftMiddle, { 255,255,255,255 }, "lives_icon", "livesContainer");

		std::string livesText = "x" + std::to_string(lives.value);

		BuildTextElement(ui, "livesText", BuildRectTransform(1.0f, 0.5f, 0, 0), Align::RightMiddle, { 255,255,255,255 }, "EuropeanTeletext|18", livesText.c_str(), "livesContainer");

		std::string timerText = "TIME " + std::to_string(ui.cachedValues["timer"]);

		BuildTextElement(ui, "timerText", BuildRectTransform(0.0f, 0.5f, 75, 0), Align::LeftMiddle, { 255,255,255,255 }, "EuropeanTeletext|18", timerText.c_str(), "topBar");

		BuildTextElement(ui, "fpsText", BuildRectTransform(1.0f, 0.5f, -75, 0), Align::RightMiddle, { 255,255,255,255 }, "EuropeanTeletext|18", "", "topBar");

		if (playAnimation) {
			ui.uiAnimator.currentTime = 0.0f;
			ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["inGameStartup"];
		}	
	}

	static void BuildGameOverUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		ui.uiElements.clear();
		ui.uiSelectables.clear();
		
		auto& gameManager = registry.view<GAME::GameManager>();
		GAME::Score score = registry.get<GAME::Score>(*(gameManager.begin()));

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 10,10,30,255 }, "background", "", 0);

		BuildTextElement(ui, "gameOverText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|48", "GAME OVER");

		BuildTextElement(ui, "newHighScoreText", BuildRectTransform(0.5f, 0.5f, 0, 100), Align::CenterTop, { 50,50,225,0 }, "EuropeanTeletext|30", "NEW HIGH SCORE!!!");

		if (!playAnimation) return;

		ui.uiAnimator.currentTime = 0.0f;
		if (GAME::SaveScore::IsTopTenScore(score.value)) {
			ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["gameOver_newHighScore"];
		}
		else {
			ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["gameOver"];
		}
	}

	static void BuildNameEntryUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		ui.uiElements.clear();
		ui.uiSelectables.clear();

		auto& gameManager = registry.view<GAME::GameManager>();
		GAME::Score score = registry.get<GAME::Score>(*(gameManager.begin()));
		

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 10,10,30,255 }, "background", "", 0);

		BuildQuadElement(ui, "nameEntryFrame", BuildRectTransform(0.5f, 0.5f, 0, 0), { 525,300 }, Align::CenterMiddle, { 0,50,230,210 }, "frame");

		BuildTextElement(ui, "nameEntryTitle", BuildRectTransform(0.5f, 0.0f, 0, 20), Align::CenterTop, { 255,255,255,255 }, "EuropeanTeletext|18", "ENTER INITIALS", "nameEntryFrame");

		BuildQuadElement(ui, "letterSlotsContainer", BuildRectTransform( 0.5f, 0.5f, 0, 40 ), { 1,1 }, Align::CenterMiddle, { 0,0,0,0 }, "default", "nameEntryFrame");

		for (int i = 0; i < 3; i++) {
			float x = (i - 1) * 140;

			std::string slotId = "letterSlot_" + std::to_string(i);

			BuildSelectableQuadElement(ui, slotId.c_str(), BuildRectTransform(0.5f, 0.5f, x, 0), { 110,140 }, Align::CenterMiddle, { 25,100,240,210 }, "frame", "letterSlotsContainer", UIElementScaleFlags::SCALE_ALL, UICommand::CONFIRM_LETTER);

			std::string previousId = "letterPrev_" + std::to_string(i);
			std::string currentId = "letterCurr_" + std::to_string(i);
			std::string nextId = "letterNext_" + std::to_string(i);

			BuildTextElement(ui, previousId.c_str(), BuildRectTransform(0.5f, 0.5f, 0, -40), Align::CenterMiddle, { 255,255,255,90 }, "EuropeanTeletext|30", "?", slotId.c_str());
			BuildTextElement(ui, currentId.c_str(), BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "EuropeanTeletext|30", "A", slotId.c_str());
			BuildTextElement(ui, nextId.c_str(), BuildRectTransform(0.5f, 0.5f, 0, 40), Align::CenterMiddle, { 255,255,255,90 }, "EuropeanTeletext|30", "B", slotId.c_str());

			UISelectable* slot = nullptr;
			for (auto& selectable : ui.uiSelectables) {
				if (selectable.uiElementId == slotId) {
					slot = &selectable;
					break;
				}
			}
			if (slot != nullptr) {
				slot->leftId = "letterSlot_" + std::to_string((i + 2) % 3);
				slot->rightId = "letterSlot_" + std::to_string((i + 1) % 3);
				slot->upId = slotId;
				slot->downId = slotId;
			}
		}

		if (!playAnimation) return;

		ui.uiSelector.currentlyFocusedId = "letterSlot_0";

		ui.nameEntry.active = true;
		ui.nameEntry.cursor = 0;
		ui.nameEntry.buffer = "AAA";
		ui.nameEntry.score = score.value;

		RefreshAllSlots(ui);

		ui.uiAnimator.currentTime = 0.0f;
		ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["nameEntry"];
	}

	static void BuildPauseUI(UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		ui.uiElements.clear();
		ui.uiSelectables.clear();

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 0,0,0,215 }, "default", "", 0);

		BuildSelectableQuadElement(ui, "resumeFrame", BuildRectTransform(0.5f, 0.5f, 0, -40), { 150, 50 }, Align::CenterMiddle, { 155,155,155,200 }, "frame", "", UIElementScaleFlags::SCALE_ALL, UICommand::PAUSE, "", "quitFrame", "quitFrame");

		BuildTextElement(ui, "resumeText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|30", "RESUME", "resumeFrame");

		BuildSelectableQuadElement(ui, "quitFrame", BuildRectTransform(0.5f, 0.5f, 0, 40), { 150, 50 }, Align::CenterMiddle, { 155,155,155,200 }, "frame", "", UIElementScaleFlags::SCALE_ALL, UICommand::QUIT, "", "resumeFrame", "resumeFrame");

		BuildTextElement(ui, "quitText", BuildRectTransform(0.5f, 0.5f, 0, 0), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|30", "QUIT", "quitFrame");

		if (!playAnimation) return;

		ui.uiSelector.currentlyFocusedId = "resumeFrame";


	}

	static void BuildCompletedUI(entt::registry& registry, UI& ui, float windowWidth, float windowHeight, bool playAnimation = true) {
		ui.uiElements.clear();
		ui.uiSelectables.clear();

		auto& gameManager = registry.view<GAME::GameManager>();
		GAME::Score score = registry.get<GAME::Score>(*(gameManager.begin()));
		GAME::Timer timer = registry.get<GAME::Timer>(*(gameManager.begin()));

		int time = static_cast<int>(std::ceil(timer.GetSeconds()));

		BuildQuadElement(ui, "background", BuildRectTransform(0.5f, 0.5f, 0, 0), { windowWidth, windowHeight }, Align::CenterMiddle, { 10,10,30,255 }, "background", "", 0);

		BuildTextElement(ui, "completedText", BuildRectTransform(0.5f, 0.5f, 0, -200), Align::CenterMiddle, { 255,255,255,255 }, "MidnightLetters|48", "ALL PARTS COLLECTED!");

		BuildQuadElement(ui, "resultsParent", BuildRectTransform(0.5f, 0.5f, 0, 100), { 500,300 }, Align::CenterMiddle, { 0,0,0,0 }, "default");

		std::string timeRemainingText = std::to_string(time);

		BuildTextElement(ui, "timeRemainingLabel", BuildRectTransform(0.0f, 0.0f, 0, 100), Align::LeftTop, { 215,215,255,255 }, "EuropeanTeletext|30", "TIME REMAINING", "resultsParent");

		BuildTextElement(ui, "timeRemainingText", BuildRectTransform(1.0f, 0.0f, 0, 100), Align::RightTop, { 255,255,255,255 }, "EuropeanTeletext|30", timeRemainingText.c_str(), "resultsParent");

		std::string scoreBonusText = "+" + std::to_string(time * 10);

		BuildTextElement(ui, "scoreBonusLabel", BuildRectTransform(0.0f, 0.0f, 0, 150), Align::LeftTop, { 215,215,255,255 }, "EuropeanTeletext|30", "SCORE BONUS", "resultsParent");

		BuildTextElement(ui, "scoreBonusText", BuildRectTransform(1.0f, 0.0f, 0, 150), Align::RightTop, { 255,255,255,255 }, "EuropeanTeletext|30", scoreBonusText.c_str(), "resultsParent");

		BuildTextElement(ui, "finalScoreLabel", BuildRectTransform(0.0, 0.0f, 0, 200), Align::LeftTop, { 245,245,255,255 }, "EuropeanTeletext|30", "FINAL SCORE", "resultsParent");

		std::string finalScoreText = std::to_string(score.value);

		BuildTextElement(ui, "finalScoreText", BuildRectTransform(1.0f, 0.0f, 0, 200), Align::RightTop, { 255,255,255,255 }, "EuropeanTeletext|30", finalScoreText.c_str(), "resultsParent");


		if (!playAnimation) return;

		ui.uiAnimator.currentTime = 0.0f;
		ui.uiAnimator.currentlyPlayingClip = &ui.uiAnimator.animations["completed"];
	}

	/*
	
	CORE UI RENDER FUNCTIONS
	
	*/


	static inline void RenderUIElements(entt::registry& registry, entt::entity entity, UIDrawList& drawList, const std::vector<UIElement>& elements, UISelector& uiSelector, unsigned int windowWidth, unsigned int windowHeight, float uiScale) {
		for (const auto& element : elements) {

			Color color = element.color;
			if (uiSelector.enabled && element.id == uiSelector.currentlyFocusedId) {
				color = TintSelected(color);
			}

			if (element.type == UIElementType::QUAD) {
				Quad quad{};

				MakeAlignedRect(element.position, element.extents, element.alignment.horizontal, element.alignment.vertical, quad.topLeft, quad.bottomRight);
				quad.topLeft = element.topLeft;
				quad.bottomRight = element.bottomRight;
				quad.color = color;

				RenderQuad(registry, entity, drawList, element.textureName, quad, windowWidth, windowHeight);
			}
			else {
				Text text{};
				text.pos = element.position;
				text.color = color;
				text.text = element.text;
				text.alignment = element.alignment;

				RenderText(registry, entity, drawList, element.fontName, text, windowWidth, windowHeight, uiScale);
			}
		}
	}

	/*
	
	CHECK USER INPUT
	
	*/


	void CheckUISelectionMovement(entt::registry& registry, UI& ui) {
		if (!ui.uiSelector.enabled) return;

		UTIL::Input& input = registry.ctx().get<UTIL::Input>();
		UTIL::DeltaTime& dt = registry.ctx().get<UTIL::DeltaTime>();
		UISelector& uiSelector = ui.uiSelector;
		
		std::string soundToPlay = "";
		float range = 0.0;

		uiSelector.currentCooldown = max(uiSelector.currentCooldown - dt.dtSec, 0);

		if (uiSelector.currentCooldown <= 0) {
			float up, down, left, right;
			input.immediateInput.GetState(G_KEY_W, up);
			input.immediateInput.GetState(G_KEY_S, down);
			input.immediateInput.GetState(G_KEY_D, right);
			input.immediateInput.GetState(G_KEY_A, left);

			uiSelector.upPress = up > 0;
			uiSelector.leftPress = left > 0;
			uiSelector.downPress = down > 0;
			uiSelector.rightPress = right > 0;

			if (ui.nameEntry.active) {
				if (!(uiSelector.upPress && uiSelector.downPress)) {
					if (uiSelector.upPress && uiSelector.upValid) {
						RotateCurrentLetter(ui, -1);
						soundToPlay = "letterWheelMove";
						range = 0.25f;
					}
					if (uiSelector.downPress && uiSelector.downValid) {

						RotateCurrentLetter(ui, 1);
						soundToPlay = "letterWheelMove";
						range = 0.25f;
					}
					uiSelector.currentCooldown = uiSelector.moveCooldown;
					
				}
			}
			else {
				if (!(uiSelector.upPress && uiSelector.downPress)) {
					if (uiSelector.upPress && uiSelector.upValid) {
						MoveFocus(ui, UIDirection::UP);
						soundToPlay = "menuSelectSound";
						range = 0.1f;
					}
					if (uiSelector.downPress && uiSelector.downValid) {
						MoveFocus(ui, UIDirection::DOWN);
						soundToPlay = "menuSelectSound";
						range = 0.1f;
						
					}
					uiSelector.currentCooldown = uiSelector.moveCooldown;
					
				}
			}

			if (!(uiSelector.leftPress && uiSelector.rightPress)) {
				if (uiSelector.leftPress && uiSelector.leftValid) {
					MoveFocus(ui, UIDirection::LEFT);
					soundToPlay = "menuSelectSound";
					range = 0.1f;
				}
				if (uiSelector.rightPress && uiSelector.rightValid) {
					MoveFocus(ui, UIDirection::RIGHT);
					soundToPlay = "menuSelectSound";
					range = 0.1f;
				}
				uiSelector.currentCooldown = uiSelector.moveCooldown;
				
			}

			uiSelector.upValid = up == 0;
			uiSelector.downValid = down == 0;
			uiSelector.rightValid = right == 0;
			uiSelector.leftValid = left == 0;

			float space;
			input.immediateInput.GetState(G_KEY_SPACE, space);
			uiSelector.spacePress = space > 0;
			if (uiSelector.spacePress && uiSelector.spaceValid) {
				ActivateSelected(registry, ui);
				uiSelector.currentCooldown = uiSelector.moveCooldown;
				soundToPlay = "Success";
				range = 0.05f;
			}
			uiSelector.spaceValid = space == 0;

		}
		
		if (!soundToPlay.empty()) {
			AUDIO::playAudio(registry, soundToPlay, 1.0f, 0.0f, 1.0f - range, 1.0f + range);
		}
		
	}

	/*
	
	UPDATE CACHED DISPLAY VALUES
	
	*/

	static void InitializeCacheValues(UI& ui) {
		ui.cachedValues.insert({ "score", 0 });
		ui.cachedValues.insert({ "lives", 0 });
		ui.cachedValues.insert({ "timer", 0 });
		ui.cachedValues.insert({ "fps", 0 });
	}

	static void UpdateCachedValues(entt::registry& registry, UI& ui) {
		if (ui.currentUIState != IN_GAME) return;

		auto view = registry.view<GAME::GameManager>();
		if (view.begin() == view.end()) return;

		auto gameManager = *view.begin();

		int score = registry.get<GAME::Score>(gameManager).value;

		if (ui.cachedValues["score"] != score) {
			ui.cachedValues["score"] = score;
			if (UIElement* element = GetElementById(ui.uiElements, "scoreText")) {
				element->text = "SCORE " + std::to_string(score);
			}
		}

		int lives = registry.get<GAME::Lives>(gameManager).value;

		if (ui.cachedValues["lives"] != lives) {
			ui.cachedValues["lives"] = lives;
			if (UIElement* element = GetElementById(ui.uiElements, "livesText")) {
				element->text = "x" + std::to_string(lives);
			}
		}

		int fps = UTIL::GetFps(registry);

		if (ui.cachedValues["fps"] != fps) {
			ui.cachedValues["fps"] = fps;
			if (UIElement* element = GetElementById(ui.uiElements, "fpsText")) {
				element->text = std::to_string(fps) + " FPS";
			}
		}

		int time = static_cast<int>(std::ceil( registry.get<GAME::Timer>(gameManager).GetSeconds()));

		if (ui.cachedValues["timer"] != time) {
			ui.cachedValues["timer"] = time;
			if (UIElement* element = GetElementById(ui.uiElements, "timerText")) {
				element->text = "TIME " + std::to_string(time);
			}
		}

	}


	/*
	
	CONNECTED COMPONENT LOGIC
	
	*/


	void Update_UI(entt::registry& registry, entt::entity entity) {
		if (registry.try_get<VulkanUIRenderer>(entity) == nullptr) return;

		auto& ui = registry.get<UI>(entity);
		auto& uiDrawList = registry.get<UIDrawList>(entity);
		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);

		unsigned int windowWidth, windowHeight;

		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);

		float width = (float)windowWidth;
		float height = (float)windowHeight;

		float dtSec = registry.ctx().get<UTIL::DeltaTime>().dtSec;

		bool stateChanged = false;
		bool playAnimation = true;

		float newScale = ComputeFit(windowWidth, windowHeight, ui.referenceWidth, ui.referenceHeight);

		if (newScale != ui.uiScale) {
			ui.uiScale = newScale;
			stateChanged = true;
			playAnimation = false;
		}

		if (ui.lastWidth != windowWidth || ui.lastHeight != windowHeight) {
			stateChanged = true;
			playAnimation = false;
			ui.lastWidth = windowWidth;
			ui.lastHeight = windowHeight;
		}

		if (UIStateChange* stateChange = registry.try_get<UIStateChange>(entity)) {
			ui.currentUIState = stateChange->newUIState;
			registry.remove<UIStateChange>(entity);
			stateChanged = true;
			playAnimation = stateChange->playAnimation;
		}
		
		if (stateChanged) {
			switch (ui.currentUIState) {
			case START_GAME: BuildStartGameUI(ui, width, height, playAnimation); break;
			case DEATH: BuildDeathUI(registry, ui, width, height, playAnimation); break;
			case IN_GAME: BuildInGameUI(registry, ui, width, height, playAnimation); break;
			case GAME_OVER: BuildGameOverUI(registry, ui, width, height, playAnimation); break;
			case HIGH_SCORE_ENTRY: BuildNameEntryUI(registry, ui, width, height, playAnimation); break;
			case PAUSE: BuildPauseUI(ui, width, height, playAnimation); break;
			case PRE_IN_GAME: BuildPreInGameUI(registry, ui, width, height, false, playAnimation); break;
			case PRE_IN_GAME_RESPAWN: BuildPreInGameUI(registry, ui, width, height, true, playAnimation); break;
			case WIN: BuildWinUI(registry, ui, width, height, playAnimation); break;
			case TIME_UP: BuildTimeUpUI(registry, ui, width, height, playAnimation); break;
			case COMPLETED: BuildCompletedUI(registry, ui, width, height, playAnimation); break;
			default: ui.uiElements.clear(); ui.uiAnimator.currentlyPlayingClip = nullptr; break;
			}
		}

		CheckUISelectionMovement(registry, ui);
		UpdateCachedValues(registry, ui);
		UpdateUIAnimator(registry, entity, ui, ui.uiAnimator, ui.uiElements, dtSec, {width, height});
		ResolveUIHierarchy(ui.uiElements, { width, height }, (float)ui.uiScale);

		uiDrawList.drawCommands.clear();
		uiDrawList.indices.clear();
		uiDrawList.vertices.clear();

		RenderUIElements(registry, entity, uiDrawList, ui.uiElements, ui.uiSelector, windowWidth, windowHeight, ui.uiScale);
	}

	void Construct_UI(entt::registry& registry, entt::entity entity) {
		auto& ui = registry.get<UI>(entity);
		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);

		unsigned int windowWidth, windowHeight;

		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);

		ui.lastWidth = windowWidth;
		ui.lastHeight = windowHeight;

		InitializeCacheValues(ui);

		CreateStartGameUIAnimation(ui.uiAnimator);
		CreateDeathUIAnimations(ui.uiAnimator);
		CreateInGameUIAnimations(ui.uiAnimator);
		CreatePreInGameUIAnimation(ui.uiAnimator);
		CreateGameOverUIAnimations(ui.uiAnimator);
		CreateWinUIAnimations(ui.uiAnimator);
		CreateNameEntryAnimations(ui.uiAnimator);
		CreateTimeUpAnimation(ui.uiAnimator);
		CreateCompletedAnimation(ui.uiAnimator);
		
		CreateStartGameUIIdleAnimation(ui.uiAnimator);

		float width = (float)windowWidth;
		float height = (float)windowHeight;
		BuildStartGameUI(ui, width, height);

	}

	CONNECT_COMPONENT_LOGIC() {
		registry.on_construct<UI>().connect<Construct_UI>();
		registry.on_update<UI>().connect<Update_UI>();
	}
}
