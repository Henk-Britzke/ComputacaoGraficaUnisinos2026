#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <assert.h>

using namespace std;

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
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

struct MaterialInfo {
    glm::vec3 Ka{0.2f};
    glm::vec3 Kd{1.0f};
    glm::vec3 Ks{0.5f};
    float Ns = 32.0f;
    string diffuseTexture;
};

// Protótipos das funções
int setupShader();
struct ObjModel;
GLuint loadSimpleOBJ(const string& filePath, int& nVertices, const glm::vec3& color, GLuint& outTextureID, bool& outTextured, MaterialInfo& outMaterial);
bool loadMTL(const string& filePath, MaterialInfo& outMaterial);
GLuint loadTexture(const string& filePath);
void updateThreePointLights(const ObjModel& mainObject);
bool loadTrajectories(const string& filePath);
bool saveTrajectories(const string& filePath);
void updateObjectTrajectory(ObjModel& obj, float deltaTime);

// Dimensões da janela (pode ser alterado em tempo de execução)
const GLuint WIDTH = 1000, HEIGHT = 1000;
const string TRAJECTORY_FILE = "trajectories.txt";

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;

    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 5.0f))
        : position(startPosition),
          front(glm::vec3(0.0f, 0.0f, -1.0f)),
          worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
          yaw(-90.0f),
          pitch(0.0f),
          speed(3.0f),
          sensitivity(0.1f)
    {
        updateVectors();
    }

    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    void Mover(GLFWwindow* window, float deltaTime)
    {
        float velocity = speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += front * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= front * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += right * velocity;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            position += worldUp * velocity;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            position -= worldUp * velocity;
    }

    void Rotacionar(float xoffset, float yoffset)
    {
        yaw += xoffset * sensitivity;
        pitch += yoffset * sensitivity;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        updateVectors();
    }

private:
    void updateVectors()
    {
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        front = glm::normalize(direction);
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};

// Código fonte do Vertex Shader (em GLSL)
const GLchar* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"layout (location = 2) in vec2 texCoord;\n"
"layout (location = 3) in vec3 normal;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"uniform bool useOverrideColor;\n"
"uniform vec4 overrideColor;\n"
"out vec4 finalColor;\n"
"out vec2 TexCoord;\n"
"out vec3 FragPos;\n"
"out vec3 Normal;\n"
"void main()\n"
"{\n"
"    vec4 worldPos = model * vec4(position, 1.0);\n"
"    gl_Position = projection * view * worldPos;\n"
"    FragPos = vec3(worldPos);\n"
"    Normal = mat3(transpose(inverse(model))) * normal;\n"
"    finalColor = useOverrideColor ? overrideColor : vec4(color, 1.0);\n"
"    TexCoord = texCoord;\n"
"}\0";

//Código fonte do Fragment Shader (em GLSL)
const GLchar* fragmentShaderSource = "#version 330 core\n"
"in vec4 finalColor;\n"
"in vec2 TexCoord;\n"
"in vec3 FragPos;\n"
"in vec3 Normal;\n"
"uniform sampler2D texBuff;\n"
"uniform bool useTexture;\n"
"const int NUM_LIGHTS = 3;\n"
"uniform vec3 lightPos[NUM_LIGHTS];\n"
"uniform vec3 lightColor[NUM_LIGHTS];\n"
"uniform float lightIntensity[NUM_LIGHTS];\n"
"uniform int lightEnabled[NUM_LIGHTS];\n"
"uniform vec3 viewPos;\n"
"uniform vec3 Ka;\n"
"uniform vec3 Kd;\n"
"uniform vec3 Ks;\n"
"uniform float Ns;\n"
"uniform float Kc;\n"
"uniform float Kl;\n"
"uniform float Kq;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"    vec3 objectColor = finalColor.rgb;\n"
"    if (useTexture) {\n"
"        objectColor = texture(texBuff, TexCoord).rgb;\n"
"    }\n"
"    vec3 N = normalize(Normal);\n"
"    vec3 V = normalize(viewPos - FragPos);\n"
"    vec3 result = vec3(0.0);\n"
"    for (int i = 0; i < NUM_LIGHTS; i++) {\n"
"        if (lightEnabled[i] == 0) {\n"
"            continue;\n"
"        }\n"
"        vec3 L = normalize(lightPos[i] - FragPos);\n"
"        vec3 R = reflect(-L, N);\n"
"        float d = length(lightPos[i] - FragPos);\n"
"        float attenuation = 1.0 / (Kc + Kl * d + Kq * d * d);\n"
"        vec3 ambient = Ka * lightColor[i] * lightIntensity[i] * objectColor;\n"
"        float diff = max(dot(N, L), 0.0);\n"
"        vec3 diffuse = Kd * diff * lightColor[i] * lightIntensity[i] * attenuation * objectColor;\n"
"        float spec = pow(max(dot(V, R), 0.0), Ns);\n"
"        vec3 specular = Ks * spec * lightColor[i] * lightIntensity[i];\n"
"        result += ambient + diffuse + specular;\n"
"    }\n"
"    color = vec4(result, finalColor.a);\n"
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
    MaterialInfo material;
    bool selected = false;
    std::vector<glm::vec3> trajectoryPoints;
    int currentTrajectoryIndex = 0;
    float trajectorySpeed = 1.2f;
    bool followTrajectory = false;
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    bool enabled = true;
};

const int NUM_LIGHTS = 3;
PointLight sceneLights[NUM_LIGHTS];
vector<ObjModel> sceneObjects;
int selectedIndex = 0;
int mainObjectIndex = 0;
Camera camera;
bool firstMouse = true;
double lastMouseX = WIDTH / 2.0;
double lastMouseY = HEIGHT / 2.0;

void printHelp()
{
    cout << "Mouse     : olhar ao redor\n";
    cout << "W/A/S/D   : mover câmera\n";
    cout << "Espaco/Shift : subir/descer câmera\n";
    cout << "Setas/I/J : transladar objeto\n";
    cout << "Z/X/C     : rotação\n";
    cout << "[]       : Escala uniforme maior|menor\n";
    cout << "ligar/desligar luz: tecla 1(principal) / tecla 2(preenchimento) / tecla 3 (fundo)\n";
    cout << "TAB             : trocar objeto selecionado\n";
    cout << "P               : adicionar ponto de trajetória\n";
    cout << "T               : ativar/desativar trajetória\n";
    cout << "O               : limpar pontos de trajetória\n";
    cout << "L               : salvar trajetórias\n";
    cout << "ESC       : sair\n";
}

ObjModel& selectedObject()
{
    assert(!sceneObjects.empty());
    return sceneObjects[selectedIndex];
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

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Trajetoria", nullptr, nullptr);
    if (!window) {
        cerr << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Failed to initialize GLAD" << endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);
    //
    //const GLubyte* renderer = glGetString(GL_RENDERER);
    //const GLubyte* version = glGetString(GL_VERSION);
    //cout << "Renderer: " << renderer << endl;
    //cout << "OpenGL version supported " << version << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);
    //criacao de variaveis para guardar as localizacoes (IDs) das variaveis uniformes no shader, para que possam ser usadas posteriormente para enviar os dados de transformacao e iluminacao para o shader
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint overrideLoc = glGetUniformLocation(shaderID, "useOverrideColor");
    GLint overrideColorLoc = glGetUniformLocation(shaderID, "overrideColor");
    GLint texUniformLoc = glGetUniformLocation(shaderID, "texBuff");
    GLint useTextureLoc = glGetUniformLocation(shaderID, "useTexture");
    GLint viewPosLoc = glGetUniformLocation(shaderID, "viewPos");
    GLint kaLoc = glGetUniformLocation(shaderID, "Ka");
    GLint kdLoc = glGetUniformLocation(shaderID, "Kd");
    GLint ksLoc = glGetUniformLocation(shaderID, "Ks");
    GLint nsLoc = glGetUniformLocation(shaderID, "Ns");
    GLint kcLoc = glGetUniformLocation(shaderID, "Kc");
    GLint klLoc = glGetUniformLocation(shaderID, "Kl");
    GLint kqLoc = glGetUniformLocation(shaderID, "Kq");
    GLint lightPosLoc[NUM_LIGHTS];
    GLint lightColorLoc[NUM_LIGHTS];
    GLint lightIntensityLoc[NUM_LIGHTS];
    GLint lightEnabledLoc[NUM_LIGHTS];
    for (int i = 0; i < NUM_LIGHTS; i++) {
        string index = to_string(i);
        lightPosLoc[i] = glGetUniformLocation(shaderID, ("lightPos[" + index + "]").c_str());
        lightColorLoc[i] = glGetUniformLocation(shaderID, ("lightColor[" + index + "]").c_str());
        lightIntensityLoc[i] = glGetUniformLocation(shaderID, ("lightIntensity[" + index + "]").c_str());
        lightEnabledLoc[i] = glGetUniformLocation(shaderID, ("lightEnabled[" + index + "]").c_str());
    }
    // //enviar para o shader.
    glUniform1i(texUniformLoc, 0);
    glUniform1i(useTextureLoc, 0);
    glUniform1f(kcLoc, 1.0f);
    glUniform1f(klLoc, 0.09f);
    glUniform1f(kqLoc, 0.032f);

    //modificacao de projecao para perspectiva
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
        0.1f,
        100.0f
    );
    //enviar matriz de projecao para o shader
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glEnable(GL_DEPTH_TEST);

    printHelp();

    {
        string path = "../assets/Modelos3D/Suzanne.obj";
        int nVertices = 0;
        GLuint textureID = 0;
        bool textured = false;
        MaterialInfo material;
        GLuint vao = loadSimpleOBJ(path, nVertices, glm::vec3(0.2f, 0.7f, 0.9f), textureID, textured, material);
        if (vao == 0 || nVertices <= 0) {
            cerr << "Falha ao carregar modelo OBJ: " << path << endl;
            glfwDestroyWindow(window);
            glfwTerminate();
            return -1;
        }

        ObjModel obj;
        obj.name = "Suzanne";
        obj.VAO = vao;
        obj.vertexCount = nVertices;
        obj.translation = glm::vec3(0.0f);
        obj.rotation = glm::vec3(0.0f);
        obj.scale = glm::vec3(1.0f);
        obj.color = glm::vec3(0.2f, 0.7f, 0.9f);
        obj.material = material;
        obj.textureID = loadTexture("../assets/Modelos3D/Suzanne.png");
        obj.textured = true;
        
        sceneObjects.push_back(obj);
    }

    {
        string path = "../assets/Modelos3D/Cube.obj";
        int nVertices = 0;
        GLuint textureID = 0;
        bool textured = false;
        MaterialInfo material;
        GLuint vao = loadSimpleOBJ(path, nVertices, glm::vec3(0.9f, 0.3f, 0.3f), textureID, textured, material);
        if (vao != 0 && nVertices > 0) {
            ObjModel obj;
            obj.name = "Cube";
            obj.VAO = vao;
            obj.vertexCount = nVertices;
            obj.translation = glm::vec3(3.0f, 0.0f, 0.0f);
            obj.rotation = glm::vec3(0.0f);
            obj.scale = glm::vec3(1.0f);
            obj.color = glm::vec3(0.9f, 0.3f, 0.3f);
            sceneObjects.push_back(obj);
        }
    }

    if (sceneObjects.empty()) {
        cerr << "Nenhum modelo carregado. Verifique os arquivos OBJ." << endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    loadTrajectories(TRAJECTORY_FILE);

    mainObjectIndex = 0;
    selectedIndex = 0;
    sceneObjects[selectedIndex].selected = true;

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        glfwPollEvents();
        camera.Mover(window, deltaTime);

        for (auto& obj : sceneObjects) {
            if (obj.followTrajectory && !obj.trajectoryPoints.empty()) {
                updateObjectTrajectory(obj, deltaTime);
            }
            obj.rotation += obj.rotationSpeed * deltaTime;
        }
        updateThreePointLights(sceneObjects[mainObjectIndex]);

        glm::mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(camera.position));

        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < NUM_LIGHTS; i++) {
            glUniform3fv(lightPosLoc[i], 1, glm::value_ptr(sceneLights[i].position));
            glUniform3fv(lightColorLoc[i], 1, glm::value_ptr(sceneLights[i].color));
            glUniform1f(lightIntensityLoc[i], sceneLights[i].intensity);
            glUniform1i(lightEnabledLoc[i], sceneLights[i].enabled ? 1 : 0);
        }

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
            glUniform3fv(kaLoc, 1, glm::value_ptr(obj.material.Ka));
            glUniform3fv(kdLoc, 1, glm::value_ptr(obj.material.Kd));
            glUniform3fv(ksLoc, 1, glm::value_ptr(obj.material.Ks));
            glUniform1f(nsLoc, obj.material.Ns);

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

    saveTrajectories(TRAJECTORY_FILE);
    for (const auto& obj : sceneObjects) {
        glDeleteVertexArrays(1, &obj.VAO);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
//configuracao das posicoes das luzes de 3 pontos com base na posicao e escala do objeto
void updateThreePointLights(const ObjModel& mainObject)
{
    float objectScale = glm::max(mainObject.scale.x, glm::max(mainObject.scale.y, mainObject.scale.z));
    float distance = glm::max(objectScale * 2.5f, 2.0f);
    glm::vec3 center = mainObject.translation;

    sceneLights[0].position = center + glm::vec3(-distance, distance * 0.9f, distance);
    sceneLights[0].color = glm::vec3(1.0f, 0.95f, 0.88f);
    sceneLights[0].intensity = 1.25f;

    sceneLights[1].position = center + glm::vec3(distance, distance * 0.45f, distance * 0.65f);
    sceneLights[1].color = glm::vec3(0.75f, 0.85f, 1.0f);
    sceneLights[1].intensity = 0.45f;

    sceneLights[2].position = center + glm::vec3(0.0f, distance * 0.8f, -distance);
    sceneLights[2].color = glm::vec3(1.0f, 1.0f, 1.0f);
    sceneLights[2].intensity = 0.85f;
}
// acao movimento do mouse
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - lastMouseX);
    float yoffset = static_cast<float>(lastMouseY - ypos);
    lastMouseX = xpos;
    lastMouseY = ypos;

    camera.Rotacionar(xoffset, yoffset);
}
//acoes do teclado
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }

    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    if (action == GLFW_PRESS && key >= GLFW_KEY_1 && key <= GLFW_KEY_3) {
        int lightIndex = key - GLFW_KEY_1;
        sceneLights[lightIndex].enabled = !sceneLights[lightIndex].enabled;
    }

    if (sceneObjects.empty()) {
        return;
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        sceneObjects[selectedIndex].selected = false;
        selectedIndex = (selectedIndex + 1) % static_cast<int>(sceneObjects.size());
        sceneObjects[selectedIndex].selected = true;
        cout << "Objeto selecionado: " << sceneObjects[selectedIndex].name << endl;
        return;
    }

    ObjModel& obj = selectedObject();

    if (key == GLFW_KEY_LEFT) {
        obj.translation.x -= 0.1f;
    }
    if (key == GLFW_KEY_RIGHT) {
        obj.translation.x += 0.1f;
    }
    if (key == GLFW_KEY_UP) {
        obj.translation.z -= 0.1f;
    }
    if (key == GLFW_KEY_DOWN) {
        obj.translation.z += 0.1f;
    }
    if (key == GLFW_KEY_I) {
        obj.translation.y += 0.1f;
    }
    if (key == GLFW_KEY_J) {
        obj.translation.y -= 0.1f;
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        obj.trajectoryPoints.push_back(obj.translation);
        cout << "Ponto de trajetoria adicionado para " << obj.name << ": ("
             << obj.translation.x << ", " << obj.translation.y << ", " << obj.translation.z << ")\n";
    }
    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        if (obj.trajectoryPoints.empty()) {
            cout << "Nenhum ponto de trajetoria para o objeto selecionado. Use P para adicionar pontos." << endl;
        } else {
            obj.followTrajectory = !obj.followTrajectory;
            cout << "Trajetoria " << (obj.followTrajectory ? "ativada" : "desativada") << " para " << obj.name << endl;
        }
    }
    if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        obj.trajectoryPoints.clear();
        obj.currentTrajectoryIndex = 0;
        obj.followTrajectory = false;
        cout << "Pontos de trajetoria removidos de " << obj.name << endl;
    }
    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        if (saveTrajectories(TRAJECTORY_FILE)) {
            cout << "Trajetorias salvas em " << TRAJECTORY_FILE << endl;
        } else {
            cout << "Falha ao salvar trajetorias em " << TRAJECTORY_FILE << endl;
        }
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


    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

bool loadTrajectories(const string& filePath)
{
    ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        istringstream ss(line);
        string name;
        float x, y, z;
        if (!(ss >> name >> x >> y >> z)) continue;

        for (auto& obj : sceneObjects) {
            if (obj.name == name) {
                obj.trajectoryPoints.emplace_back(x, y, z);
                break;
            }
        }
    }
    file.close();
    return true;
}

bool saveTrajectories(const string& filePath)
{
    ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    for (const auto& obj : sceneObjects) {
        for (const auto& point : obj.trajectoryPoints) {
            file << obj.name << " " << point.x << " " << point.y << " " << point.z << "\n";
        }
    }
    file.close();
    return true;
}

void updateObjectTrajectory(ObjModel& obj, float deltaTime)
{
    if (obj.trajectoryPoints.empty()) {
        return;
    }
    if (obj.currentTrajectoryIndex < 0 || obj.currentTrajectoryIndex >= static_cast<int>(obj.trajectoryPoints.size())) {
        obj.currentTrajectoryIndex = 0;
    }

    const glm::vec3 target = obj.trajectoryPoints[obj.currentTrajectoryIndex];
    glm::vec3 direction = target - obj.translation;
    float distance = glm::length(direction);
    if (distance < 0.01f) {
        obj.currentTrajectoryIndex = (obj.currentTrajectoryIndex + 1) % static_cast<int>(obj.trajectoryPoints.size());
        return;
    }

    glm::vec3 movement = glm::normalize(direction) * obj.trajectorySpeed * deltaTime;
    if (glm::length(movement) >= distance) {
        obj.translation = target;
        obj.currentTrajectoryIndex = (obj.currentTrajectoryIndex + 1) % static_cast<int>(obj.trajectoryPoints.size());
    } else {
        obj.translation += movement;
    }
}

bool loadMTL(const string& filePath, MaterialInfo& outMaterial)
{
    ifstream file(filePath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir arquivo MTL: " << filePath << endl;
        return false;
    }

    outMaterial = MaterialInfo();
    string line;
    string word;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        istringstream ss(line);
        ss >> word;
        if (word == "Ka") {
            ss >> outMaterial.Ka.r >> outMaterial.Ka.g >> outMaterial.Ka.b;
        } else if (word == "Kd") {
            ss >> outMaterial.Kd.r >> outMaterial.Kd.g >> outMaterial.Kd.b;
        } else if (word == "Ks") {
            ss >> outMaterial.Ks.r >> outMaterial.Ks.g >> outMaterial.Ks.b;
        } else if (word == "Ns") {
            ss >> outMaterial.Ns;
        } else if (word == "map_Kd") {
            ss >> outMaterial.diffuseTexture;
        }
    }

    file.close();
    return true;
}

// Esta função `loadTexture` é responsável por carregar uma imagem de um arquivo e criar uma textura OpenGL a partir dela, retornando o identificador da textura (GLuint) para uso posterior na aplicação. A função utiliza a biblioteca stb_image para ler os dados da imagem e configurar os parâmetros da textura no OpenGL.
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

// Esta função `loadSimpleOBJ`, responsável por carregar arquivos no formato Wavefront .OBJ e armazenar seus vértices em um VAO para renderização
// com OpenGL. FAZ A MESMA COISA QUE O setupGeometry(), MAS CARREGANDO A GEOMETRIA DE UM ARQUIVO EXTERNO AO INVÉS DE USAR VERTICES HARDCODED
GLuint loadSimpleOBJ(const string& filePATH, int& nVertices, const glm::vec3& color, GLuint& outTextureID, bool& outTextured, MaterialInfo& outMaterial)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat> vBuffer;

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
            string mtlFile;
            ssline >> mtlFile;
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
                glm::vec3 normal = (ni >= 0 && ni < static_cast<int>(normals.size())) ? normals[ni] : glm::vec3(0.0f, 0.0f, 1.0f);

                vBuffer.push_back(position.x);
                vBuffer.push_back(position.y);
                vBuffer.push_back(position.z);
                vBuffer.push_back(color.r);
                vBuffer.push_back(color.g);
                vBuffer.push_back(color.b);
                vBuffer.push_back(texCoord.x);
                vBuffer.push_back(texCoord.y);
                vBuffer.push_back(normal.x);
                vBuffer.push_back(normal.y);
                vBuffer.push_back(normal.z);
            }
        }
    }

    arqEntrada.close();

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    // atributos de posicao
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    // atributos de cor 
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    // atributos de coordenada de textura
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    // atributos de normal
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(8 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 11;

    return VAO;
}
