#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <assert.h>

using namespace std;
namespace fs = std::filesystem;

// GLAD
#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Protótipo da função de callback de teclado
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

// Protótipos das funções
int setupShader();
GLuint loadSimpleOBJ(const string& filePath, int& nVertices, const glm::vec3& color, GLuint& outTextureID, bool& outTextured);
GLuint loadTexture(const string& filePath);
bool loadMtlFile(const string& filePath, unordered_map<string,string>& materialTextureMap);
string getDirectory(const string& filePath);
string joinPath(const string& directory, const string& relativePath);
string resolveModelPath(const string& relativePath);

// Dimensões da janela (pode ser alterado em tempo de execução)
const GLuint WIDTH = 1000, HEIGHT = 1000;

// Código fonte do Vertex Shader (em GLSL)
const GLchar* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"layout (location = 2) in vec2 texCoord;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"uniform bool useOverrideColor;\n"
"uniform vec4 overrideColor;\n"
"out vec4 finalColor;\n"
"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
"    gl_Position = projection * view * model * vec4(position, 1.0);\n"
"    finalColor = useOverrideColor ? overrideColor : vec4(color, 1.0);\n"
"    TexCoord = texCoord;\n"
"}\0";

const GLchar* fragmentShaderSource = "#version 330 core\n"
"in vec4 finalColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D texBuff;\n"
"uniform bool useTexture;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"    if (useTexture) {\n"
"        color = texture(texBuff, TexCoord);\n"
"    } else {\n"
"        color = finalColor;\n"
"    }\n"
"}\n\0";

struct ObjModel {
    string name;
    GLuint VAO = 0;
    GLuint textureID = 0;
    bool textured = false;
    int vertexCount = 0;
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 rotationSpeed{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 color{1.0f};
    bool selected = false;
};

vector<ObjModel> sceneObjects;
int selectedIndex = 0;

string resolveModelPath(const string& relativePath)
{
    vector<string> candidates;
    candidates.push_back(relativePath);
    candidates.push_back("../" + relativePath);
    candidates.push_back("assets/Modelos3D/" + fs::path(relativePath).filename().string());
    candidates.push_back("../assets/Modelos3D/" + fs::path(relativePath).filename().string());

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return relativePath;
}

void printHelp()
{
    cout << "TAB       : selecionar próximo objeto\n";
    cout << "W/A/S/D/I/J   : transladar\n";
    cout << "Z/X/C     : rotação\n";
    cout << "[]       : Escala uniforme maior|menor\n";
    cout << "ESC       : sair\n";
}

ObjModel& selectedObject()
{
    assert(!sceneObjects.empty());
    return sceneObjects[selectedIndex];
}

GLuint loadSceneObject(const string& filePath, const string& name, const glm::vec3& color, const glm::vec3& translation)
{
    string path = resolveModelPath(filePath);
    int nVertices = 0;
    GLuint textureID = 0;
    bool textured = false;
    GLuint vao = loadSimpleOBJ(path, nVertices, color, textureID, textured);
    if (vao == 0 || nVertices <= 0) {
        cerr << "Falha ao carregar modelo OBJ: " << path << endl;
        return 0;
    }

    ObjModel obj;
    obj.name = name;
    obj.VAO = vao;
    obj.textureID = textureID;
    obj.textured = textured;
    obj.vertexCount = nVertices;
    obj.translation = translation;
    obj.rotation = glm::vec3(0.0f);
    obj.scale = glm::vec3(1.0f);
    obj.color = color;
    sceneObjects.push_back(obj);
    return vao;
}

int main()
{
    if (!glfwInit()) {
        cerr << "Falha ao inicializar GLFW" << endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Atividade Vivencial1", nullptr, nullptr);
    if (!window) {
        cerr << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Failed to initialize GLAD" << endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported " << version << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);

    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint overrideLoc = glGetUniformLocation(shaderID, "useOverrideColor");
    GLint overrideColorLoc = glGetUniformLocation(shaderID, "overrideColor");
    GLint texUniformLoc = glGetUniformLocation(shaderID, "texBuff");
    GLint useTextureLoc = glGetUniformLocation(shaderID, "useTexture");
    glUniform1i(texUniformLoc, 0);
    glUniform1i(useTextureLoc, 0);

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
        0.1f,
        100.0f
    );
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glEnable(GL_DEPTH_TEST);

    printHelp();

    loadSceneObject("assets/Modelos3D/Cube.obj", "Cube", glm::vec3(0.8f, 0.3f, 0.2f), glm::vec3(-2.0f, 0.0f, 0.0f));
    loadSceneObject("assets/Modelos3D/Suzanne.obj", "Suzanne", glm::vec3(0.2f, 0.7f, 0.9f), glm::vec3(2.0f, 0.0f, 0.0f));

    if (sceneObjects.empty()) {
        cerr << "Nenhum modelo carregado. Verifique os arquivos OBJ." << endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    selectedIndex = 0;
    sceneObjects[selectedIndex].selected = true;

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        glfwPollEvents();

        for (auto& obj : sceneObjects) {
            obj.rotation += obj.rotationSpeed * deltaTime;
        }

        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (auto& obj : sceneObjects) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, obj.translation);
            model = glm::rotate(model, obj.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, obj.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, obj.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, obj.scale);

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(overrideLoc, obj.selected ? 1 : 0);
            if (obj.selected) {
                glUniform4f(overrideColorLoc, 1.0f, 1.0f, 0.2f, 1.0f);
            }

            glUniform1i(useTextureLoc, obj.textured ? 1 : 0);
            if (obj.textured && obj.textureID != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj.textureID);
            } else {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glBindVertexArray(obj.VAO);
            glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount);
            glBindVertexArray(0);

            if (obj.textured && obj.textureID != 0) {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }

        glfwSwapBuffers(window);
    }

    for (const auto& obj : sceneObjects) {
        glDeleteVertexArrays(1, &obj.VAO);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }

    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    if (sceneObjects.empty()) {
        return;
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        sceneObjects[selectedIndex].selected = false;
        selectedIndex = (selectedIndex + 1) % static_cast<int>(sceneObjects.size());
        sceneObjects[selectedIndex].selected = true;
        return;
    }

    ObjModel& obj = selectedObject();

    if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) {
        obj.translation.x -= 0.1f;
    }
    if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) {
        obj.translation.x += 0.1f;
    }
    if (key == GLFW_KEY_W) {
        obj.translation.z -= 0.1f;
    }
    if (key == GLFW_KEY_S) {
        obj.translation.z += 0.1f;
    }
    if (key == GLFW_KEY_I) {
        obj.translation.y += 0.1f;
    }
    if (key == GLFW_KEY_J) {
        obj.translation.y -= 0.1f;
    }

    if (key == GLFW_KEY_Z && action == GLFW_PRESS) {
        obj.rotation = glm::vec3(0.0f);
        obj.rotationSpeed = glm::vec3(0.0f);
        obj.rotationSpeed.z = glm::radians(90.0f);
    }
    if (key == GLFW_KEY_X && action == GLFW_PRESS) {
        obj.rotation = glm::vec3(0.0f);
        obj.rotationSpeed = glm::vec3(0.0f);
        obj.rotationSpeed.x = glm::radians(90.0f);
    }
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        obj.rotation = glm::vec3(0.0f);
        obj.rotationSpeed = glm::vec3(0.0f);
        obj.rotationSpeed.y = glm::radians(90.0f);
    }

    if (key == GLFW_KEY_LEFT_BRACKET) {
        obj.scale *= 1.1f;
    }
    if (key == GLFW_KEY_RIGHT_BRACKET) {
        obj.scale *= 0.9f;
    }
}

int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

string getDirectory(const string& filePath)
{
    return fs::path(filePath).remove_filename().string();
}

string joinPath(const string& directory, const string& relativePath)
{
    if (relativePath.empty()) return directory;
    if (fs::path(relativePath).is_absolute()) return relativePath;
    return (fs::path(directory) / relativePath).string();
}

bool loadMtlFile(const string& filePath, unordered_map<string,string>& materialTextureMap)
{
    ifstream mtlFile(filePath);
    if (!mtlFile.is_open()) {
        return false;
    }

    string currentMaterial;
    string line;
    while (getline(mtlFile, line)) {
        istringstream ss(line);
        string keyword;
        ss >> keyword;
        if (keyword == "newmtl") {
            ss >> currentMaterial;
        } else if (keyword == "map_Kd" && !currentMaterial.empty()) {
            string textureName;
            ss >> textureName;
            if (!textureName.empty()) {
                materialTextureMap[currentMaterial] = textureName;
            }
        }
    }
    return true;
}

GLuint loadTexture(const string& filePath)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        cerr << "Erro ao carregar textura: " << filePath << endl;
        return 0;
    }

    GLenum format = GL_RGB;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    return textureID;
}

GLuint loadSimpleOBJ(const string& filePATH, int& nVertices, const glm::vec3& color, GLuint& outTextureID, bool& outTextured)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat> vBuffer;
    unordered_map<string,string> materialTextureMap;
    string currentMaterial;
    string directory = getDirectory(filePATH);

    ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open()) 
	{
        cerr << "Erro ao tentar ler o arquivo " << filePATH << endl;
        return 0;
    }

    string line;
    while (getline(arqEntrada, line)) 
	{
        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "v") 
		{
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        } 
        else if (word == "vt") 
		{
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        } 
        else if (word == "vn") 
		{
            glm::vec3 normal;
            ssline >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } 
        else if (word == "mtllib") {
            string mtlName;
            ssline >> mtlName;
            string mtlPath = joinPath(directory, mtlName);
            loadMtlFile(mtlPath, materialTextureMap);
        } else if (word == "usemtl") {
            ssline >> currentMaterial;
        } else if (word == "f")
		 {
            while (ssline >> word) 
		 {
                int vi = 0, ti = 0, ni = 0;
                istringstream ss(word);
                string index;

                if (getline(ss, index, '/')) vi = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index, '/')) ti = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index)) ni = !index.empty() ? stoi(index) - 1 : 0;

                glm::vec3 position = (vi >= 0 && vi < static_cast<int>(vertices.size())) ? vertices[vi] : glm::vec3(0.0f);
                glm::vec2 texCoord = (ti >= 0 && ti < static_cast<int>(texCoords.size())) ? texCoords[ti] : glm::vec2(0.0f);

                vBuffer.push_back(position.x);
                vBuffer.push_back(position.y);
                vBuffer.push_back(position.z);
                vBuffer.push_back(color.r);
                vBuffer.push_back(color.g);
                vBuffer.push_back(color.b);
                vBuffer.push_back(texCoord.x);
                vBuffer.push_back(texCoord.y);
            }
        }
    }

    arqEntrada.close();

    if (!currentMaterial.empty() && materialTextureMap.count(currentMaterial) > 0) {
        string texName = materialTextureMap[currentMaterial];
        string texPath = joinPath(directory, texName);
        outTextureID = loadTexture(texPath);
        outTextured = outTextureID != 0;
    } else {
        outTextureID = 0;
        outTextured = false;
    }

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 8;

    return VAO;
}

	