#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>

struct Config {
    std::string inputPath;
    std::string targetPath = "truffle.png"; // the secret sauce
};

struct Particle {
    sf::Vector2f startPos;
    sf::Vector2f endPos;
    sf::Color startColor;
    sf::Color endColor;
    sf::Vector2f currentPos;
    sf::Color currentColor;
    float size;
};

// cubic moves only
float easeOutCubic(float x) {
    return 1.0f - pow(1.0f - x, 3.0f);
}

class Trufflifier {
private:
    std::vector<Particle> particles;
    sf::VertexArray vertexArray;
    float baseParticleSize = 1.0f;

    sf::Image inputImage;
    sf::Image targetImage;

public:
    Trufflifier() : vertexArray(sf::Quads) {}

    bool load(const std::string& inputPath, const std::string& targetPath) {
        if (!inputImage.loadFromFile(inputPath)) {
            std::cout << "Failed to load input: " << inputPath << std::endl;
            return false;
        }

        // try to load truffle, fallback if not found
        if (!targetImage.loadFromFile(targetPath)) {
            std::cout << "Failed to load target (truffle.png). Make sure it is in the same directory." << std::endl;
            return false;
        }
        return true;
    }

    long long colorDiff(const sf::Color& c1, const sf::Color& c2) {
        long long dr = (long long)c1.r - c2.r;
        long long dg = (long long)c1.g - c2.g;
        long long db = (long long)c1.b - c2.b;
        return dr*dr + dg*dg + db*db;
    }

    void initParticles(unsigned int windowW, unsigned int windowH) {
        particles.clear();

        // 1. where do we want them to go?
        float targetAspect = (float)targetImage.getSize().x / targetImage.getSize().y;
        float windowAspect = (float)windowW / windowH;

        float targetScale;
        if (targetAspect > windowAspect) {
            targetScale = (windowW * 0.8f) / targetImage.getSize().x;
        } else {
            targetScale = (windowH * 0.8f) / targetImage.getSize().y;
        }
        targetScale = std::max(1.0f, targetScale);

        float targetOffsetX = (windowW - targetImage.getSize().x * targetScale) / 2.0f;
        float targetOffsetY = (windowH - targetImage.getSize().y * targetScale) / 2.0f;

        // 2. where do they start?
        // if the input is huge, we need to chill and downsample
        // aiming for ~15k
        unsigned int inW = inputImage.getSize().x;
        unsigned int inH = inputImage.getSize().y;
        float maxParticles = 15000.0f;
        float ratio = std::sqrt(maxParticles / (inW * inH));
        // skip pixels if too many
        int step = 1;
        if (ratio < 1.0f) {
            step = (int)(1.0f / ratio);
        }

        // calculate display size so the input fits
        float displayScaleX = (float)windowW / inW;
        float displayScaleY = (float)windowH / inH;
        float displayScale = std::min(displayScaleX, displayScaleY) * 0.8f; // 80% of window cause margins are nice

        float inputOffsetX = (windowW - inW * displayScale) / 2.0f;
        float inputOffsetY = (windowH - inH * displayScale) / 2.0f;

        // make particles fatter if we skipped pixels
        baseParticleSize = std::max(1.0f, displayScale * step);

        // pre-calculate target spots so we can find them fast
        // list of {color, pos} basically
        struct TargetPixel {
            sf::Color color;
            sf::Vector2f pos;
        };
        std::vector<TargetPixel> targetPixels;
        targetPixels.reserve(targetImage.getSize().x * targetImage.getSize().y);

        for(unsigned int y=0; y<targetImage.getSize().y; ++y) {
             for(unsigned int x=0; x<targetImage.getSize().x; ++x) {
                 sf::Color c = targetImage.getPixel(x, y);
                 if(c.a > 0) {
                     targetPixels.push_back({c, sf::Vector2f(targetOffsetX + x * targetScale, targetOffsetY + y * targetScale)});
                 }
             }
        }

        std::mt19937 rng(std::random_device{}()); // this is disgusting

        std::uniform_real_distribution<float> jitterDist(-targetScale * 0.4f, targetScale * 0.4f); // make it messy
        std::uniform_real_distribution<float> sizeDist(0.8f, 1.2f); // also size dist

        // 3. iterate over input image
        for (unsigned int y = 0; y < inH; y += step) {
            for (unsigned int x = 0; x < inW; x += step) {
                sf::Color inputCol = inputImage.getPixel(x, y);

                // invisible pixels are skipped
                if(inputCol.a == 0) continue;

                Particle p;
                p.startPos = sf::Vector2f(inputOffsetX + x * displayScale, inputOffsetY + y * displayScale);
                p.startColor = inputCol;
                p.currentColor = inputCol;
                p.size = baseParticleSize * sizeDist(rng); // random fat

                // find best match in target
                if (targetPixels.empty()) {
                    p.endPos = p.startPos;
                } else {
                    // checking every single pixel is slow
                    // checking random ones is fast and usually good enough
                    // chaos theory
                    int bestIndex = 0;
                    long long minDiff = -1;

                    int samples = 100;
                    std::uniform_int_distribution<int> distT(0, targetPixels.size() - 1);

                    for(int i=0; i<samples; ++i) {
                        int idx = distT(rng);
                        long long diff = colorDiff(inputCol, targetPixels[idx].color);
                        if (minDiff == -1 || diff < minDiff) {
                            minDiff = diff;
                            bestIndex = idx;
                        }
                    }

                    // jitter the end pos so it doesnt look like a boring grid
                    p.endPos = targetPixels[bestIndex].pos;
                    p.endPos.x += jitterDist(rng);
                    p.endPos.y += jitterDist(rng);

                    p.endColor = targetPixels[bestIndex].color;
                }

                particles.push_back(p);
            }
        }
    }

    void update(float t) {
        float easedT = easeOutCubic(t);
        vertexArray.clear();

        for (auto& p : particles) {
            p.currentPos = p.startPos + (p.endPos - p.startPos) * easedT;

            // keep original color, no mixing allowed
            p.currentColor = p.startColor;

            float currentSize = p.size;

            sf::Vertex v1, v2, v3, v4;
            v1.position = p.currentPos;
            v2.position = sf::Vector2f(p.currentPos.x + currentSize, p.currentPos.y);
            v3.position = sf::Vector2f(p.currentPos.x + currentSize, p.currentPos.y + currentSize);
            v4.position = sf::Vector2f(p.currentPos.x, p.currentPos.y + currentSize);

            v1.color = p.currentColor;
            v2.color = p.currentColor;
            v3.color = p.currentColor;
            v4.color = p.currentColor;

            vertexArray.append(v1);
            vertexArray.append(v2);
            vertexArray.append(v3);
            vertexArray.append(v4);
        }
    }

    void draw(sf::RenderWindow& window) {
        window.draw(vertexArray);
    }
};

int main(int argc, char* argv[]) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f" && i + 1 < argc) config.inputPath = argv[i + 1];
    }

    if (config.inputPath.empty()) {
        std::cout << "Usage: trufflify.exe -f \"image.png\"" << std::endl;
        return 1;
    }

    sf::RenderWindow window(sf::VideoMode(800, 800), "Trufflify");
    window.setFramerateLimit(60);

    Trufflifier app;
    if (!app.load(config.inputPath, config.targetPath)) return -1;

    app.initParticles(window.getSize().x, window.getSize().y);

    sf::Clock clock;
    float duration = 3.0f; // 3 seconds seems about right
    bool isRunning = false;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) window.close();

                if (event.key.code == sf::Keyboard::Enter) {
                    if (!isRunning) {
                        isRunning = true;
                        clock.restart();
                    } else {
                        window.close();
                    }
                }

                if (event.key.code == sf::Keyboard::R && isRunning) {
                    clock.restart();
                }
            }
        }

        float progress = 0.0f;
        if (isRunning) {
            float time = clock.getElapsedTime().asSeconds();
            progress = std::min(time / duration, 1.0f);
        }

        app.update(progress);

        window.clear(sf::Color(20, 20, 30));
        app.draw(window);
        window.display();
    }
    return 0;
}