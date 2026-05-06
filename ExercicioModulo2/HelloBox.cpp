#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

using namespace std;

// GLAD
#include <glad/glad.h>

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
int setupGeometry();

// Dimensões da janela (pode ser alterado em tempo de execução)
const GLuint WIDTH = 1000, HEIGHT = 1000;

// Código fonte do Vertex Shader (em GLSL): ainda hardcoded
const GLchar* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
//...pode ter mais linhas de código aqui!
"gl_Position = projection * view * model * vec4(position, 1.0);\n"
"finalColor = vec4(color, 1.0);\n"
"}\0";

//Códifo fonte do Fragment Shader (em GLSL): ainda hardcoded
const GLchar* fragmentShaderSource = "#version 330 core\n"
"in vec4 finalColor;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"color = finalColor;\n"
"}\n\0";

bool rotateX=false, rotateY=false, rotateZ=false;

struct Cube {
    glm::vec3 translation;
    float scale;
    bool rotateX, rotateY, rotateZ;
};

// Variáveis de controle do cubo principal
glm::vec3 translation(0.0f, 0.0f, 0.0f);
float scale = 1.0f;

// Lista de cubos
std::vector<Cube> cubes;

// Função MAIN
int main()
{
	// Inicialização da GLFW
	if (!glfwInit()) {
		std::cerr << "Falha ao inicializar GLFW" << std::endl;
		return -1;
	}

	//Muita atenção aqui: alguns ambientes não aceitam essas configurações
	//Você deve adaptar para a versão do OpenGL suportada por sua placa
	//Sugestão: comente essas linhas de código para desobrir a versão e
	//depois atualize (por exemplo: 4.5 com 4 e 5)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//Essencial para computadores da Apple
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// Criação da janela GLFW
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Ola Cubo -- Joao Henrique Britzke!", nullptr, nullptr);
	if (!window) {
		std::cerr << "Falha ao criar janela GLFW" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Fazendo o registro da função de callback para a janela GLFW
	glfwSetKeyCallback(window, key_callback);

	// GLAD: carrega todos os ponteiros d funções da OpenGL
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cerr << "Failed to initialize GLAD" << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	// Obtendo as informações de versão
	const GLubyte* renderer = glGetString(GL_RENDERER); /* get renderer string */
	const GLubyte* version = glGetString(GL_VERSION); /* version as a string */
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported " << version << endl;

	// Definindo as dimensões da viewport com as mesmas dimensões da janela da aplicação
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);


	// Compilando e buildando o programa de shader
	GLuint shaderID = setupShader();

	// Gerando um buffer simples, com a geometria de um triângulo
	GLuint VAO = setupGeometry();


	glUseProgram(shaderID);

	glm::mat4 model = glm::mat4(1); //matriz identidade;
	GLint modelLoc = glGetUniformLocation(shaderID, "model");
	GLint viewLoc = glGetUniformLocation(shaderID, "view");
	GLint projLoc = glGetUniformLocation(shaderID, "projection");

	// Configurar câmera
	glm::mat4 view = glm::lookAt(
		glm::vec3(0.0f, 0.0f, 5.0f), // posição da câmera
		glm::vec3(0.0f, 0.0f, 0.0f), // ponto para onde olha
		glm::vec3(0.0f, 1.0f, 0.0f)  // vetor up
	);
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

	// Configurar projeção perspectiva
	glm::mat4 projection = glm::perspective(
		glm::radians(45.0f),
		(float)WIDTH / (float)HEIGHT, 
		0.1f,
		100.0f
	);
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	glEnable(GL_DEPTH_TEST);

	// Inicializar cubos
	Cube cube1 = {glm::vec3(1.5f, 0.0f, 0.0f), 0.5f, false, false, false};
	cubes.push_back(cube1);

	// Loop da aplicação - "game loop"
	while (!glfwWindowShouldClose(window))
	{
		// Checa se houveram eventos de input (key pressed, mouse moved etc.) e chama as funções de callback correspondentes
		glfwPollEvents();

		// Limpa o buffer de cor
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); //cor de fundo
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLineWidth(10);
		glPointSize(20);

		float angle = (GLfloat)glfwGetTime();

		// Desenhar cubo controlável
		{
			glm::mat4 model = glm::mat4(1);
			model = glm::translate(model, translation);
			model = glm::scale(model, glm::vec3(scale));
			
			if (rotateX)
			{
				model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
			}
			else if (rotateY)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			else if (rotateZ)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
			}

			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			glBindVertexArray(VAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
			glDrawArrays(GL_POINTS, 0, 36);
			glBindVertexArray(0);
		}

		for (const auto& cube : cubes)
		{
			glm::mat4 model = glm::mat4(1);
			model = glm::translate(model, cube.translation);
			model = glm::scale(model, glm::vec3(cube.scale));
			
			if (cube.rotateX)
			{
				model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
			}
			else if (cube.rotateY)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			else if (cube.rotateZ)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
			}

			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			glBindVertexArray(VAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
			glDrawArrays(GL_POINTS, 0, 36);
			glBindVertexArray(0);
		}

		// Troca os buffers da tela
		glfwSwapBuffers(window);
	}
	// Pede pra OpenGL desalocar os buffers
	glDeleteVertexArrays(1, &VAO);
	// Finaliza a execução da GLFW, limpando os recursos alocados por ela
	glfwTerminate();
	return 0;
}

// Função de callback de teclado - só pode ter uma instância (deve ser estática se
// estiver dentro de uma classe) - É chamada sempre que uma tecla for pressionada
// ou solta via GLFW
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	// Controles de rotação
	if (key == GLFW_KEY_X && action == GLFW_PRESS)
	{
		rotateX = true;
		rotateY = false;
		rotateZ = false;
	}

	if (key == GLFW_KEY_Y && action == GLFW_PRESS)
	{
		rotateX = false;
		rotateY = true;
		rotateZ = false;
	}

	if (key == GLFW_KEY_Z && action == GLFW_PRESS)
	{
		rotateX = false;
		rotateY = false;
		rotateZ = true;
	}

	// Controles de translação
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		// Controles de translação - Eixo X (A/D)
		if (key == GLFW_KEY_A)
			translation.x -= 0.1f;
		if (key == GLFW_KEY_D)
			translation.x += 0.1f;

		// Controles de translação - Eixo Z (W/S)
		if (key == GLFW_KEY_W)
			translation.z -= 0.1f;
		if (key == GLFW_KEY_S)
			translation.z += 0.1f;

		// Controles de translação - Eixo Y (I/J)
		if (key == GLFW_KEY_I)
			translation.y += 0.1f;
		if (key == GLFW_KEY_J)
			translation.y -= 0.1f;

		// Controles de escala ([ para diminuir, ] para aumentar)
		if (key == GLFW_KEY_LEFT_BRACKET && scale > 0.2f)
			scale -= 0.1f;
		if (key == GLFW_KEY_RIGHT_BRACKET && scale < 3.0f)
			scale += 0.1f;
	}
}

//Esta função está basntante hardcoded - objetivo é compilar e "buildar" um programa de
// shader simples e único neste exemplo de código
// O código fonte do vertex e fragment shader está nos arrays vertexShaderSource e
// fragmentShader source no iniçio deste arquivo
// A função retorna o identificador do programa de shader
int setupShader()
{
	// Vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	// Checando erros de compilação (exibição via log no terminal)
	GLint success;
	GLchar infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// Fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	// Checando erros de compilação (exibição via log no terminal)
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// Linkando os shaders e criando o identificador do programa de shader
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	// Checando por erros de linkagem
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

// Esta função está bastante harcoded - objetivo é criar os buffers que armazenam a 
// geometria de um triângulo
// Apenas atributo coordenada nos vértices
// 1 VBO com as coordenadas, VAO com apenas 1 ponteiro para atributo
// A função retorna o identificador do VAO
int setupGeometry()
{
	// Aqui setamos as coordenadas x, y e z do triângulo e as armazenamos de forma
	// sequencial, já visando mandar para o VBO (Vertex Buffer Objects)
	// Cada atributo do vértice (coordenada, cores, coordenadas de textura, normal, etc)
	// Pode ser arazenado em um VBO único ou em VBOs separados
	GLfloat vertices[] = {

		// cubo com 6 faces coloridas
		// x    y    z      r    g    b

		// frente vermelho
		-0.5, -0.5,  0.5,  1.0, 0.0, 0.0,
		 0.5, -0.5,  0.5,  1.0, 0.0, 0.0,
		 0.5,  0.5,  0.5,  1.0, 0.0, 0.0,
		 0.5,  0.5,  0.5,  1.0, 0.0, 0.0,
		-0.5,  0.5,  0.5,  1.0, 0.0, 0.0,
		-0.5, -0.5,  0.5,  1.0, 0.0, 0.0,

		// traseira verde
		-0.5, -0.5, -0.5,  0.0, 1.0, 0.0,
		-0.5,  0.5, -0.5,  0.0, 1.0, 0.0,
		 0.5,  0.5, -0.5,  0.0, 1.0, 0.0,
		 0.5,  0.5, -0.5,  0.0, 1.0, 0.0,
		 0.5, -0.5, -0.5,  0.0, 1.0, 0.0,
		-0.5, -0.5, -0.5,  0.0, 1.0, 0.0,

		// esquerda azul
		-0.5, -0.5, -0.5,  0.0, 0.0, 1.0,
		-0.5, -0.5,  0.5,  0.0, 0.0, 1.0,
		-0.5,  0.5,  0.5,  0.0, 0.0, 1.0,
		-0.5,  0.5,  0.5,  0.0, 0.0, 1.0,
		-0.5,  0.5, -0.5,  0.0, 0.0, 1.0,
		-0.5, -0.5, -0.5,  0.0, 0.0, 1.0,

		// direita amarelo
		 0.5, -0.5, -0.5,  1.0, 1.0, 0.0,
		 0.5,  0.5, -0.5,  1.0, 1.0, 0.0,
		 0.5,  0.5,  0.5,  1.0, 1.0, 0.0,
		 0.5,  0.5,  0.5,  1.0, 1.0, 0.0,
		 0.5, -0.5,  0.5,  1.0, 1.0, 0.0,
		 0.5, -0.5, -0.5,  1.0, 1.0, 0.0,

		// topo magenta
		-0.5,  0.5, -0.5,  1.0, 0.0, 1.0,
		-0.5,  0.5,  0.5,  1.0, 0.0, 1.0,
		 0.5,  0.5,  0.5,  1.0, 0.0, 1.0,
		 0.5,  0.5,  0.5,  1.0, 0.0, 1.0,
		 0.5,  0.5, -0.5,  1.0, 0.0, 1.0,
		-0.5,  0.5, -0.5,  1.0, 0.0, 1.0,

		// fundo ciano
		-0.5, -0.5, -0.5,  0.0, 1.0, 1.0,
		 0.5, -0.5, -0.5,  0.0, 1.0, 1.0,
		 0.5, -0.5,  0.5,  0.0, 1.0, 1.0,
		 0.5, -0.5,  0.5,  0.0, 1.0, 1.0,
		-0.5, -0.5,  0.5,  0.0, 1.0, 1.0,
		-0.5, -0.5, -0.5,  0.0, 1.0, 1.0,

	};

	GLuint VBO, VAO;

	//Geração do identificador do VBO
	glGenBuffers(1, &VBO);

	//Faz a conexão (vincula) do buffer como um buffer de array
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	//Envia os dados do array de floats para o buffer da OpenGl
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	//Geração do identificador do VAO (Vertex Array Object)
	glGenVertexArrays(1, &VAO);

	// Vincula (bind) o VAO primeiro, e em seguida  conecta e seta o(s) buffer(s) de vértices
	// e os ponteiros para os atributos 
	glBindVertexArray(VAO);
	
	//Para cada atributo do vertice, criamos um "AttribPointer" (ponteiro para o atributo), indicando: 
	// Localização no shader * (a localização dos atributos devem ser correspondentes no layout especificado no vertex shader)
	// Numero de valores que o atributo tem (por ex, 3 coordenadas xyz) 
	// Tipo do dado
	// Se está normalizado (entre zero e um)
	// Tamanho em bytes 
	// Deslocamento a partir do byte zero 
	
	//Atributo posição (x, y, z)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	//Atributo cor (r, g, b)
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(1);


	// Observe que isso é permitido, a chamada para glVertexAttribPointer registrou o VBO como o objeto de buffer de vértice 
	// atualmente vinculado - para que depois possamos desvincular com segurança
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Desvincula o VAO (é uma boa prática desvincular qualquer buffer ou array para evitar bugs medonhos)
	glBindVertexArray(0);

	return VAO;
}

