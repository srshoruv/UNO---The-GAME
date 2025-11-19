#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <iostream>
#include <cmath>
#include <map>

using namespace std;

enum CardColor { RED, GREEN, BLUE, YELLOW, NONE };
enum CardType { NUMBER, SKIP, REVERSE, DRAW_TWO, WILD, WILD_DRAW_FOUR };
enum GameState { PLAYER_TURN, AI_TURN, AI_THINKING, WILD_COLOR_SELECT, ANIMATING_PLAYER_PLAY, ANIMATING_PLAYER_DRAW, ANIMATING_AI_PLAY, ANIMATING_AI_DRAW, GAME_OVER_PLAYER_WON, GAME_OVER_AI_WON };

class Card {
    public:
    CardColor color;
    CardType type;
    int number;
    float x, y;

    bool isAnimating = false;
    double animDuration = 0.5;
    double currentAnimTime = 0.0;

    float startX = 0.0f, startY = 0.0f;
    float targetX = 0.0f, targetY = 0.0f;
};

float cardW = 0.15f, cardH = 0.22f;
vector<Card> playerHand;
vector<Card> aiHand;
vector<Card> drawPile;
vector<Card> discardPile;

GameState gameState = PLAYER_TURN;
CardColor wildSelectedColor = NONE;
double aiThinkingStartTime;

bool canSelectWildColor = false;

map<string, GLuint> textures;
GLuint backgroundTextureID;
GLuint playerAvatarID;
GLuint aiAvatarID;
GLuint crownTextureID;

GLuint loadTexture(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrChannels == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        cout << "Failed to load texture: " << path << endl;
        textureID = 0;
    }
    stbi_image_free(data);
    return textureID;
}


float cardVerts[] = {
    -0.5f, -0.7f,    0.0f, 0.0f,
     0.5f, -0.7f,    1.0f, 0.0f,
     0.5f,  0.7f,    1.0f, 1.0f,
    -0.5f,  0.7f,    0.0f, 1.0f
};
unsigned int indices[] = {0, 1, 2, 2, 3, 0};

float ui_verts[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 1.0f
};

float backgroundVertices[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};
unsigned int backgroundIndices[] = {0, 1, 2, 2, 3, 0};


const char* vtxSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform vec2 offset;
uniform vec2 scale;

void main() {
    gl_Position = vec4(aPos * scale + offset, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";
const char* fragSrc = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform vec3 color;
uniform float highlight;
uniform sampler2D ourTexture;
uniform int hasTexture;
uniform int isWild;

void main() {
    vec4 texColor = texture(ourTexture, TexCoord);
    vec4 baseColor = vec4(color, 1.0);

    vec4 finalColor = texColor;

    if (hasTexture > 0) {
        if (texColor.r > 0.9 && texColor.g > 0.9 && texColor.b > 0.9) {
            finalColor = baseColor;
        }
    }

    if (highlight > 0.5)
        FragColor = vec4(finalColor.rgb * 0.7 + vec3(0.3,0.3,0.3), 1.0);
    else
        FragColor = finalColor;
}
)";
const char* uiVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 position;
uniform vec2 size;
uniform float alpha;
void main() {
    vec2 pos = aPos * size + position;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";
const char* uiFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
uniform float alpha;
void main() {
    FragColor = vec4(color, alpha);
}
)";

GLuint createShader(const char* vertexSource, const char* fragmentSource) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexSource, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentSource, nullptr);
    glCompileShader(fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}


void nextTurn();
void layoutHand();
void layoutAIHand();


void colorToRGB(CardColor c, float& r, float& g, float& b) {
    switch(c) {
        case RED: r=1; g=0.0f; b=0.0f; break;
        case GREEN: r=0.0f; g=1; b=0.0f; break;
        case BLUE: r=0.0f; g=0.0f; b=1; break;
        case YELLOW: r=1; g=1; b=0.2f; break;
        default: r=g=b=0.7f;
    }
}

string cardColorToString(CardColor c) {
    switch(c) {
        case RED: return "red";
        case GREEN: return "green";
        case BLUE: return "blue";
        case YELLOW: return "yellow";
        default: return "";
    }
}

GLuint getCardTexture(const Card& c) {
    string key;
    if (c.type == WILD || c.type == WILD_DRAW_FOUR) {
        key = "textures/wild/";
        if (c.type == WILD) key += "wild.png";
        else key += "wild_draw.png";
    } else {
        string colorStr = cardColorToString(c.color);
        key = "textures/" + colorStr + "/";
        switch (c.type) {
            case NUMBER: key += to_string(c.number) + "_" + colorStr + ".png"; break;
            case SKIP: key += "block_" + colorStr + ".png"; break;
            case REVERSE: key += "inverse_" + colorStr + ".png"; break;
            case DRAW_TWO: key += "2plus_" + colorStr + ".png"; break;
            default: key = "textures/card_back/back.png";
        }
    }

    if (textures.find(key) == textures.end()) {
        cerr << "Texture not found for key: " << key << endl;
        return 0;
    }
    return textures[key];
}

vector<Card> makeDeck() {
    vector<Card> deck;
    for (int c = 0; c < 4; ++c) {
        for (int n = 0; n <= 9; ++n) {
            Card card = { (CardColor)c, NUMBER, n };
            deck.push_back(card);
            if(n!=0) deck.push_back(card);
        }
        for (int i = 0; i < 2; ++i) {
            deck.push_back({ (CardColor)c, SKIP, -1 });
            deck.push_back({ (CardColor)c, REVERSE, -1 });
            deck.push_back({ (CardColor)c, DRAW_TWO, -1 });
        }
    }
    for (int i = 0; i < 4; ++i) {
        deck.push_back({ NONE, WILD, -1 });
        deck.push_back({ NONE, WILD_DRAW_FOUR, -1 });
    }
    return deck;
}
void shuffle(vector<Card>& v) {
    static random_device rd;
    static mt19937 g(rd());
    shuffle(v.begin(), v.end(), g);
}

void layoutPiles() {
    if (!drawPile.empty()) {
        drawPile.back().x = -0.7f;
        drawPile.back().y = 0.0f;
    }
    if (!discardPile.empty()) {
        discardPile.back().x = -0.3f;
        discardPile.back().y = 0.0f;
    }
}

void layoutHand() {
    if (playerHand.empty()) return;

    float spacing = 0.1f;
    float totalWidth = (playerHand.size() - 1) * spacing;
    float startX = -totalWidth / 2.0f;

    for (size_t i = 0; i < playerHand.size(); ++i) {
        playerHand[i].x = startX + i * spacing;
        playerHand[i].y = -0.7f;
    }
}

void layoutAIHand() {
    if (aiHand.empty()) return;

    float spacing = 0.1f;
    float totalWidth = (aiHand.size() - 1) * spacing;
    float startX = -totalWidth / 2.0f;

    for (size_t i = 0; i < aiHand.size(); ++i) {
        aiHand[i].x = startX + i * spacing;
        aiHand[i].y = 0.7f;
    }
}
bool canPlay(const Card& card, const Card& top) {
    if (card.type == WILD || card.type == WILD_DRAW_FOUR) return true;
    if (card.color == top.color) return true;
    if (card.type == top.type && card.type != NUMBER) return true;
    if (card.type == NUMBER && top.type == NUMBER && card.number == top.number) return true;
    return false;
}

void nextTurn() {
    if (gameState == PLAYER_TURN || gameState == ANIMATING_PLAYER_PLAY || gameState == ANIMATING_PLAYER_DRAW) {
        gameState = AI_THINKING;
        aiThinkingStartTime = glfwGetTime();
    } else {
        gameState = PLAYER_TURN;
    }
}


void applyCardEffect(const Card& playedCard) {
    if (playedCard.type == DRAW_TWO) {
        if (gameState == ANIMATING_PLAYER_PLAY) {
            for(int i = 0; i < 2; ++i) {
                if (drawPile.empty()) break;
                aiHand.push_back(drawPile.back());
                drawPile.pop_back();
            }
            layoutAIHand();
        } else {
            for(int i = 0; i < 2; ++i) {
                if (drawPile.empty()) break;
                playerHand.push_back(drawPile.back());
                drawPile.pop_back();
            }
            layoutHand();
        }
        nextTurn();
    } else if (playedCard.type == SKIP || playedCard.type == REVERSE) {
        nextTurn();
        nextTurn();
    } else {
        nextTurn();
    }

    if (playerHand.empty()) {
        gameState = GAME_OVER_PLAYER_WON;
        cout << "Player Won!\n";
    } else if (aiHand.empty()) {
        gameState = GAME_OVER_AI_WON;
        cout << "AI Won!\n";
    }
    layoutPiles();
}
void startCardAnimation(Card& card, float targetX, float targetY) {
    card.startX = card.x;
    card.startY = card.y;
    card.targetX = targetX;
    card.targetY = targetY;
    card.isAnimating = true;
    card.currentAnimTime = 0.0;
}

void updateAnimations(float deltaTime) {
    if (gameState == ANIMATING_PLAYER_PLAY) {
        Card& card = discardPile.back();
        card.currentAnimTime += deltaTime;
        float progress = min(1.0f, (float)(card.currentAnimTime / card.animDuration));

        card.x = card.startX + (card.targetX - card.startX) * progress;
        card.y = card.startY + (card.targetY - card.startY) * progress;

        if (progress >= 1.0f) {
            card.x = card.targetX;
            card.y = card.targetY;
            card.isAnimating = false;

            if (card.type == WILD || card.type == WILD_DRAW_FOUR) {
                if (card.type == WILD_DRAW_FOUR) {
                    for(int i = 0; i < 4; ++i) {
                        if (drawPile.empty()) break;
                        aiHand.push_back(drawPile.back());
                        drawPile.pop_back();
                    }
                    layoutAIHand();
                }
                gameState = WILD_COLOR_SELECT;
                discardPile.back().color = NONE;

                canSelectWildColor = true;
            } else {
                applyCardEffect(card);
            }
            layoutHand();
            layoutPiles();
        }
    }

    if (gameState == ANIMATING_PLAYER_DRAW) {
        Card& card = playerHand.back();
        card.currentAnimTime += deltaTime;
        float progress = min(1.0f, (float)(card.currentAnimTime / card.animDuration));

        card.x = card.startX + (card.targetX - card.startX) * progress;
        card.y = card.startY + (card.targetY - card.startY) * progress;

        if (progress >= 1.0f) {
            card.x = card.targetX;
            card.y = card.targetY;
            card.isAnimating = false;
            nextTurn();
            layoutHand();
            layoutPiles();
        }
    }

    if (gameState == ANIMATING_AI_PLAY) {
        Card& card = discardPile.back();
        card.currentAnimTime += deltaTime;
        float progress = min(1.0f, (float)(card.currentAnimTime / card.animDuration));

        card.x = card.startX + (card.targetX - card.startX) * progress;
        card.y = card.startY + (card.targetY - card.startY) * progress;

        if (progress >= 1.0f) {
            card.x = card.targetX;
            card.y = card.targetY;
            card.isAnimating = false;

            if (card.type == WILD_DRAW_FOUR) {
                for(int i = 0; i < 4; ++i) {
                    if (drawPile.empty()) break;
                    playerHand.push_back(drawPile.back());
                    drawPile.pop_back();
                }
                layoutHand();
            } else if (card.type != WILD) {
                applyCardEffect(card);
            }

            layoutAIHand();
            layoutPiles();
            if (card.type == WILD || card.type == WILD_DRAW_FOUR) {
                nextTurn();
            }
        }
    }

    if (gameState == ANIMATING_AI_DRAW) {
        Card& card = aiHand.back();
        card.currentAnimTime += deltaTime;
        float progress = min(1.0f, (float)(card.currentAnimTime / card.animDuration));

        card.x = card.startX + (card.targetX - card.startX) * progress;
        card.y = card.startY + (card.targetY - card.startY) * progress;

        if (progress >= 1.0f) {
            card.x = card.targetX;
            card.y = card.targetY;
            card.isAnimating = false;
            nextTurn();
            layoutAIHand();
            layoutPiles();
        }
    }
}


void aiTurn() {
    Card& top = discardPile.back();
    int playIndex = -1;
    for(size_t i = 0; i < aiHand.size(); ++i) {
        if(canPlay(aiHand[i], top)) {
            playIndex = i;
            break;
        }
    }

    if (playIndex != -1) {
        Card playedCard = aiHand[playIndex];

        playedCard.x = aiHand[playIndex].x;
        playedCard.y = aiHand[playIndex].y;

        discardPile.push_back(playedCard);
        aiHand.erase(aiHand.begin() + playIndex);

        if (playedCard.type == WILD || playedCard.type == WILD_DRAW_FOUR) {
            int colorCount[4] = {0, 0, 0, 0};
            for(const auto& card : aiHand) {
                if (card.color != NONE) {
                    colorCount[card.color]++;
                }
            }
            int maxCount = 0;
            int maxColor = 0;
            for(int i = 0; i < 4; ++i) {
                if (colorCount[i] > maxCount) {
                    maxCount = colorCount[i];
                    maxColor = i;
                }
            }
            discardPile.back().color = (CardColor)maxColor;
        }

        startCardAnimation(discardPile.back(), -0.3f, 0.0f);
        gameState = ANIMATING_AI_PLAY;
        layoutPiles();

    } else {
        if(!drawPile.empty()) {
            Card drawnCard = drawPile.back();
            drawPile.pop_back();

            drawnCard.x = -0.7f;
            drawnCard.y = 0.0f;
            aiHand.push_back(drawnCard);

            startCardAnimation(aiHand.back(), aiHand.back().x, aiHand.back().y);
            gameState = ANIMATING_AI_DRAW;
            layoutPiles();
        } else {
            nextTurn();
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (gameState != PLAYER_TURN &&
        (gameState != ANIMATING_PLAYER_PLAY ||
         (discardPile.empty() || (discardPile.back().type != WILD && discardPile.back().type != WILD_DRAW_FOUR))) &&
        gameState != WILD_COLOR_SELECT) {
        return;
    }

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    float x = (mx / width) * 2.0f - 1.0f;
    float y = 1.0f - (my / height) * 2.0f;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (gameState == PLAYER_TURN) {
            Card& top = discardPile.back();

            if (!drawPile.empty()) {
                Card& pileCard = drawPile.back();
                if (abs(x - pileCard.x) < cardW * 0.5f && abs(y - pileCard.y) < cardH * 0.5f) {
                    Card drawnCard = drawPile.back();
                    drawPile.pop_back();
                    drawnCard.x = pileCard.x;
                    drawnCard.y = pileCard.y;
                    playerHand.push_back(drawnCard);

                    layoutPiles();
                    layoutHand();
                    startCardAnimation(playerHand.back(), playerHand.back().x, playerHand.back().y);
                    gameState = ANIMATING_PLAYER_DRAW;
                    return;
                }
            }

            for (size_t i = playerHand.size(); i-- > 0; ) {
                Card& card = playerHand[i];
                if (abs(x - card.x) < cardW * 0.5f && abs(y - card.y) < cardH * 0.5f) {
                    if (canPlay(card, top)) {
                        Card playedCard = card;

                        playedCard.x = card.x;
                        playedCard.y = card.y;

                        discardPile.push_back(playedCard);
                        playerHand.erase(playerHand.begin() + i);

                        startCardAnimation(discardPile.back(), -0.3f, 0.0f);
                        gameState = ANIMATING_PLAYER_PLAY;
                        layoutPiles();
                        return;
                    }
                }
            }
        }

        if ((gameState == WILD_COLOR_SELECT ||
            (gameState == ANIMATING_PLAYER_PLAY && !discardPile.empty() &&
             (discardPile.back().type == WILD || discardPile.back().type == WILD_DRAW_FOUR))) && canSelectWildColor) {

            if (y > 0.1f && y < 0.3f) {
                CardColor selectedColor = NONE;
                if (x > -0.6f && x < -0.4f) {
                    selectedColor = RED;
                } else if (x > -0.2f && x < 0.0f) {
                    selectedColor = GREEN;
                } else if (x > 0.2f && x < 0.4f) {
                    selectedColor = BLUE;
                } else if (x > 0.6f && x < 0.8f) {
                    selectedColor = YELLOW;
                }
                if (selectedColor != NONE) {
                    discardPile.back().color = selectedColor;
                    nextTurn();
                    layoutPiles();
                    canSelectWildColor = false;
                }
            }
        }
    }
}


int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* window = glfwCreateWindow(900, 600, "UNO - OpenGL Core Profile", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    stbi_set_flip_vertically_on_load(true);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint shaderProg = createShader(vtxSrc, fragSrc);
    GLuint uiShader = createShader(uiVertexShaderSource, uiFragmentShaderSource);

    for (int c = 0; c < 4; ++c) {
        string colorStr = cardColorToString((CardColor)c);
        for (int n = 0; n <= 9; ++n) {
            string path = "textures/" + colorStr + "/" + to_string(n) + "_" + colorStr + ".png";
            textures[path] = loadTexture(path.c_str());
        }
        string pathSkip = "textures/" + colorStr + "/block_" + colorStr + ".png";
        textures[pathSkip] = loadTexture(pathSkip.c_str());
        string pathReverse = "textures/" + colorStr + "/inverse_" + colorStr + ".png";
        textures[pathReverse] = loadTexture(pathReverse.c_str());
        string pathDrawTwo = "textures/" + colorStr + "/2plus_" + colorStr + ".png";
        textures[pathDrawTwo] = loadTexture(pathDrawTwo.c_str());
    }
    textures["textures/wild/wild.png"] = loadTexture("textures/wild/wild.png");
    textures["textures/wild/wild_draw.png"] = loadTexture("textures/wild/wild_draw.png");
    textures["textures/card_back/back.png"] = loadTexture("textures/card_back/back.png");

    backgroundTextureID = loadTexture("textures/background.png");
    playerAvatarID = loadTexture("textures/player_avatar.png");
    aiAvatarID = loadTexture("textures/ai_avatar.png");
    crownTextureID = loadTexture("textures/crown.png");

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cardVerts), cardVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ui_verts), ui_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint backgroundVAO, backgroundVBO, backgroundEBO;
    glGenVertexArrays(1, &backgroundVAO);
    glGenBuffers(1, &backgroundVBO);
    glGenBuffers(1, &backgroundEBO);
    glBindVertexArray(backgroundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, backgroundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(backgroundVertices), backgroundVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backgroundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(backgroundIndices), backgroundIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLint offsetLoc = glGetUniformLocation(shaderProg, "offset");
    GLint scaleLoc = glGetUniformLocation(shaderProg, "scale");
    GLint colorLoc = glGetUniformLocation(shaderProg, "color");
    GLint highlightLoc = glGetUniformLocation(shaderProg, "highlight");
    GLint hasTextureLoc = glGetUniformLocation(shaderProg, "hasTexture");
    GLint isWildLoc = glGetUniformLocation(shaderProg, "isWild");

    GLint uiPosLoc = glGetUniformLocation(uiShader, "position");
    GLint uiSizeLoc = glGetUniformLocation(uiShader, "size");
    GLint uiColorLoc = glGetUniformLocation(uiShader, "color");
    GLint uiAlphaLoc = glGetUniformLocation(uiShader, "alpha");


    vector<Card> deck = makeDeck();
    shuffle(deck);
    playerHand.clear();
    aiHand.clear();

    for (int i = 0; i < 7; ++i) {
        playerHand.push_back(deck.back());
        deck.pop_back();
        aiHand.push_back(deck.back());
        deck.pop_back();
    }
    drawPile = deck;
    discardPile.clear();
    discardPile.push_back(drawPile.back());
    drawPile.pop_back();
    layoutHand();
    layoutAIHand();
    layoutPiles();

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (gameState != GAME_OVER_PLAYER_WON && gameState != GAME_OVER_AI_WON) {
            updateAnimations(deltaTime);
            if (gameState == AI_THINKING) {
                if (glfwGetTime() - aiThinkingStartTime > 1.0) {
                    aiTurn();
                }
            }
        }

        glUseProgram(shaderProg);
        glBindVertexArray(backgroundVAO);
        glBindTexture(GL_TEXTURE_2D, backgroundTextureID);
        glUniform2f(offsetLoc, 0.0f, 0.0f);
        glUniform2f(scaleLoc, 1.0f, 1.0f);
        glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
        glUniform1f(highlightLoc, 0.0f);
        glUniform1i(hasTextureLoc, 1);
        glUniform1i(isWildLoc, 0);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        float avatarSize = 0.15f;
        float indicatorSize = 0.22f;

        if (gameState == PLAYER_TURN || gameState == ANIMATING_PLAYER_PLAY || gameState == WILD_COLOR_SELECT) {
            glUseProgram(uiShader);
            glBindVertexArray(uiVAO);
            glUniform2f(uiPosLoc, 0.0f - indicatorSize * 0.5f, -0.35f - indicatorSize * 0.5f);
            glUniform2f(uiSizeLoc, indicatorSize, indicatorSize);
            glUniform3f(uiColorLoc, 1.0f, 1.0f, 0.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        if (gameState == AI_TURN || gameState == AI_THINKING || gameState == ANIMATING_AI_PLAY) {
            glUseProgram(uiShader);
            glBindVertexArray(uiVAO);
            glUniform2f(uiPosLoc, 0.0f - indicatorSize * 0.5f, 0.35f - indicatorSize * 0.5f);
            glUniform2f(uiSizeLoc, indicatorSize, indicatorSize);
            glUniform3f(uiColorLoc, 1.0f, 1.0f, 0.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glUseProgram(shaderProg);
        glBindVertexArray(VAO);

        glBindTexture(GL_TEXTURE_2D, playerAvatarID);
        glUniform2f(offsetLoc, 0.0f, -0.35f);
        glUniform2f(scaleLoc, avatarSize, avatarSize);
        glUniform1f(highlightLoc, 0.0f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindTexture(GL_TEXTURE_2D, aiAvatarID);
        glUniform2f(offsetLoc, 0.0f, 0.35f);
        glUniform2f(scaleLoc, avatarSize, avatarSize);
        glUniform1f(highlightLoc, 0.0f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glUseProgram(shaderProg);
        glBindVertexArray(VAO);
        if (!drawPile.empty()) {
            glUniform2f(offsetLoc, drawPile.back().x, drawPile.back().y);
            glUniform2f(scaleLoc, cardW, cardH);
            glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
            glUniform1f(highlightLoc, 0.0f);
            glUniform1i(hasTextureLoc, 1);
            glUniform1i(isWildLoc, 0);
            glBindTexture(GL_TEXTURE_2D, textures["textures/card_back/back.png"]);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        if (!discardPile.empty()) {
            float r, g, b;
            Card& top = discardPile.back();
            colorToRGB(top.color, r, g, b);
            glUniform2f(offsetLoc, top.x, top.y);
            glUniform2f(scaleLoc, cardW, cardH);
            glUniform3f(colorLoc, r, g, b);
            glUniform1f(highlightLoc, 0.0f);
            glUniform1i(hasTextureLoc, 1);
            glUniform1i(isWildLoc, top.type == WILD || top.type == WILD_DRAW_FOUR);
            glBindTexture(GL_TEXTURE_2D, getCardTexture(top));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        for (const auto& card : playerHand) {
            float r, g, b;
            colorToRGB(card.color, r, g, b);
            glUniform2f(offsetLoc, card.x, card.y);
            glUniform2f(scaleLoc, cardW, cardH);
            glUniform3f(colorLoc, r, g, b);
            glUniform1f(highlightLoc, 0.0f);
            glUniform1i(hasTextureLoc, 1);
            glUniform1i(isWildLoc, card.type == WILD || card.type == WILD_DRAW_FOUR);
            glBindTexture(GL_TEXTURE_2D, getCardTexture(card));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        for (const auto& card : aiHand) {
            glUniform2f(offsetLoc, card.x, card.y);
            glUniform2f(scaleLoc, cardW, cardH);
            glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
            glUniform1f(highlightLoc, 0.0f);
            glUniform1i(hasTextureLoc, 1);
            glUniform1i(isWildLoc, 0);
            glBindTexture(GL_TEXTURE_2D, textures["textures/card_back/back.png"]);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        glUseProgram(uiShader);
        glBindVertexArray(uiVAO);
        glUniform1f(uiAlphaLoc, 1.0f);
        if (gameState == WILD_COLOR_SELECT) {
            float uiW = 0.2f;
            float uiH = 0.2f;

            glUniform2f(uiPosLoc, -0.6f, 0.1f);
            glUniform2f(uiSizeLoc, uiW, uiH);
            glUniform3f(uiColorLoc, 1.0f, 0.2f, 0.2f);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glUniform2f(uiPosLoc, -0.2f, 0.1f);
            glUniform2f(uiSizeLoc, uiW, uiH);
            glUniform3f(uiColorLoc, 0.2f, 1.0f, 0.2f);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glUniform2f(uiPosLoc, 0.2f, 0.1f);
            glUniform2f(uiSizeLoc, uiW, uiH);
            glUniform3f(uiColorLoc, 0.2f, 0.4f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glUniform2f(uiPosLoc, 0.6f, 0.1f);
            glUniform2f(uiSizeLoc, uiW, uiH);
            glUniform3f(uiColorLoc, 1.0f, 1.0f, 0.2f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glUseProgram(shaderProg);
        glBindVertexArray(VAO);
        float crownSize = 0.1f;
        glUniform1f(highlightLoc, 0.0f);
        glUniform1i(hasTextureLoc, 1);
        glUniform1i(isWildLoc, 0);
        glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, crownTextureID);
        if (gameState == GAME_OVER_PLAYER_WON) {
            glUniform2f(offsetLoc, 0.0f, -0.35f + avatarSize/2 + crownSize/2);
            glUniform2f(scaleLoc, crownSize, crownSize);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        } else if (gameState == GAME_OVER_AI_WON) {
            glUniform2f(offsetLoc, 0.0f, 0.35f + avatarSize/2 + crownSize/2);
            glUniform2f(scaleLoc, crownSize, crownSize);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);
    glDeleteVertexArrays(1, &backgroundVAO);
    glDeleteBuffers(1, &backgroundVBO);
    glDeleteBuffers(1, &backgroundEBO);
    glDeleteProgram(shaderProg);
    glDeleteProgram(uiShader);
    glfwTerminate();
    return 0;
}