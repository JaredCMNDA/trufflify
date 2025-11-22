#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>

struct Config {
    std::string inputPath;
    std::string targetPath = "truffle.png";
};

struct Gene {
    float x, y;
    float radius;
    sf::Color color;
};

// utility functions

bool fileExists(const std::string &name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    }
    return false;
}

long long calculateColorDiff(const sf::Color& c1, const sf::Color& c2) {
    long long dr = (long long)c1.r - c2.r;
    long long dg = (long long)c1.g - c2.g;
    long long db = (long long)c1.b - c2.b;
    return dr*dr + dg*dg + db*db;
}


// main

class Trufflifier {
private:
    sf::Image targetImage;
    sf::Image currentImage;
    sf::Texture displayTexture;
    sf::Sprite displaySprite;
    unsigned int width;
    unsigned int height;
    std::mt19937 rng;

public:
    Trufflifier() : rng(std::random_device{}()) {}

    bool loadImages(const std::string& inputPath, const std::string& targetPath) {
        if (fileExists(targetPath)) {
            if (!targetImage.loadFromFile(targetPath)) return false;
        } else {
            std::cout << "secret sauce not found, aborting" << std::endl;
            return false;
        }

        width = targetImage.getSize().x;
        height = targetImage.getSize().y;

        if (fileExists(inputPath)) {
            sf::Image temp;
            if (temp.loadFromFile(inputPath)) {
                currentImage.create(width, height);
                // crop/copy
                unsigned int copyW = std::min(width, temp.getSize().x);
                unsigned int copyH = std::min(height, temp.getSize().y);
                currentImage.copy(temp, 0, 0, sf::IntRect(0, 0, copyW, copyH));
            }
        } else {
            std::cout << "no file at input path, aborting" << std::endl;
            return false;
        }

        displayTexture.loadFromImage(currentImage);
        displaySprite.setTexture(displayTexture);

        // scale up if small
        if (width < 600) {
            float scale = 600.0f / width;
            displaySprite.setScale(scale, scale);
        }
        return true;
    }

    void evolve(int iterations) {
        std::uniform_int_distribution<int> distX(0, width - 1);
        std::uniform_int_distribution<int> distY(0, height - 1);
        std::uniform_int_distribution<int> distRadius(2, 30);
        std::uniform_int_distribution<int> distColor(0, 255);

        for (int i = 0; i < iterations; ++i) {
            Gene gene;
            gene.x = (float)distX(rng);
            gene.y = (float)distY(rng);
            gene.radius = (float)distRadius(rng);
            gene.color = sf::Color(distColor(rng), distColor(rng), distColor(rng), 100);

            int minX = std::max(0, (int)(gene.x - gene.radius));
            int maxX = std::min((int)width, (int)(gene.x + gene.radius));
            int minY = std::max(0, (int)(gene.y - gene.radius));
            int maxY = std::min((int)height, (int)(gene.y + gene.radius));

            long long errorBefore = 0;
            long long errorAfter = 0;
            std::vector<sf::Color> backupPixels;

            // Calculate and Draw
            for (int y = minY; y < maxY; ++y) {
                for (int x = minX; x < maxX; ++x) {
                    float dx = x - gene.x;
                    float dy = y - gene.y;

                    if (dx*dx + dy*dy <= gene.radius*gene.radius) {
                        sf::Color current = currentImage.getPixel(x, y);
                        sf::Color target = targetImage.getPixel(x, y);

                        backupPixels.push_back(current);
                        errorBefore += calculateColorDiff(current, target);

                        // Blend
                        sf::Color blended;
                        unsigned int a = gene.color.a;
                        blended.r = (a * gene.color.r + (255 - a) * current.r) / 255;
                        blended.g = (a * gene.color.g + (255 - a) * current.g) / 255;
                        blended.b = (a * gene.color.b + (255 - a) * current.b) / 255;
                        blended.a = 255;

                        currentImage.setPixel(x, y, blended);
                        errorAfter += calculateColorDiff(blended, target);
                    }
                }
            }

            // Selection
            if (errorAfter >= errorBefore) {
                // Revert
                int idx = 0;
                for (int y = minY; y < maxY; ++y) {
                    for (int x = minX; x < maxX; ++x) {
                        float dx = x - gene.x;
                        float dy = y - gene.y;
                        if (dx*dx + dy*dy <= gene.radius*gene.radius) {
                            currentImage.setPixel(x, y, backupPixels[idx++]);
                        }
                    }
                }
            }
        }
        displayTexture.update(currentImage);
    }

    void draw(sf::RenderWindow& window) {
        window.draw(displaySprite);
    }
};

// entry

int main(int argc, char* argv[]) {
    Config config;

    // Argument Parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f" && i + 1 < argc) config.inputPath = argv[i + 1];
    }

    if (config.inputPath.empty()) {
        std::cout << "Usage: Trufflify.exe -f \"image.png\"" << std::endl;
        // Don't exit immediately so user can see the message
        std::cin.get();
        return 1;
    }

    Trufflifier app;
    if (!app.loadImages(config.inputPath, config.targetPath)) return -1;

    sf::RenderWindow window(sf::VideoMode(800, 800), "Trufflify");
    window.setFramerateLimit(60);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
        }

        app.evolve(200); // 200 mutations per frame

        window.clear(sf::Color(30, 30, 30));
        app.draw(window);
        window.display();
    }
    return 0;
}