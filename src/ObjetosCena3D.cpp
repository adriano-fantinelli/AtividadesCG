/* ObjetosCena3D - Instanciando objetos na cena 3D */

#include <iostream>
#include <string>
#include <assert.h>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Protótipo da função de callback de teclado.
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

// Protótipos das funções.
int setupShader();
int setupGeometry();

// Dimensões da janela (pode ser alterado em tempo de execução).
const GLuint WIDTH = 1000, HEIGHT = 1000;

// Código fonte do Vertex Shader.
const GLchar* vertexShaderSource = "#version 450\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"uniform mat4 model;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"gl_Position = model * vec4(position, 1.0);\n"
"finalColor = vec4(color, 1.0);\n"
"}\0";

//Código fonte do Fragment Shader.
const GLchar* fragmentShaderSource = "#version 450\n"
"in vec4 finalColor;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"color = finalColor;\n"
"}\n\0";

// Variáveis de controle de transformação.
bool rotateX=false, rotateY=false, rotateZ=false;
float translateX = 0.0f, translateY = 0.0f, translateZ = 0.0f;
float scale = 1.0f;
const float TRANSLATE_SPEED = 0.1f;
const float SCALE_SPEED = 0.1f;

int main()
{
	// Inicialização da GLFW.
	glfwInit();

	// Criação da janela GLFW.
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Objetos Cena 3D - Cubo -- Adriano Fantinelli!", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Fazendo o registro da função de callback para a janela GLFW.
	glfwSetKeyCallback(window, key_callback);

	// GLAD: carrega todos os ponteiros d funções da OpenGL.
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
	}

	// Obtendo as informações de versão.
	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported " << version << endl;

	// Definindo as dimensões da viewport com as mesmas dimensões da janela da aplicação.
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	// Compilando e buildando o programa de shader.
	GLuint shaderID = setupShader();

	// Gerando um buffer simples, com a geometria de um cubo.
	GLuint VAO = setupGeometry();

	glUseProgram(shaderID);

	GLint modelLoc = glGetUniformLocation(shaderID, "model");

	glEnable(GL_DEPTH_TEST);

	while (!glfwWindowShouldClose(window))
	{
		// Checa se houveram eventos de input e chama as funções de callback correspondentes.
		glfwPollEvents();

		// Limpa o buffer de cor.
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLineWidth(2);
		glPointSize(10);

		float angle = (GLfloat)glfwGetTime();

		glBindVertexArray(VAO);

		// Desenhar múltiplos cubos em posições diferentes.
		// Cubo 1 - Centro.
		glm::mat4 model = glm::mat4(1.0f);
		
		// Aplicar translação.
		model = glm::translate(model, glm::vec3(translateX, translateY, translateZ));
		
		// Aplicar rotação.
		if (rotateX)
		{
			model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
		}
		else if (rotateY)
		{
			model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
		}
		else if (rotateZ)
		{
			model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		}
		
		// Aplicar escala.
		model = glm::scale(model, glm::vec3(scale, scale, scale));
		
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// Cubo 2.
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-1.5f, 1.0f, 0.0f));
		model = glm::rotate(model, angle * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// Cubo 3.
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(1.5f, 1.0f, 0.0f));
		model = glm::rotate(model, angle * 0.7f, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// Cubo 4.
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, -1.5f, 0.0f));
		model = glm::rotate(model, angle * 0.3f, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		glDrawArrays(GL_TRIANGLES, 0, 36);

		glBindVertexArray(0);

		// Troca os buffers da tela.
		glfwSwapBuffers(window);
	}
	// Pede pra OpenGL desalocar os buffers.
	glDeleteVertexArrays(1, &VAO);
	// Finaliza a execução da GLFW, limpando os recursos alocados.
	glfwTerminate();
	return 0;
}

// Função de callback de teclado.
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	// Controles de rotação.
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

	// Controles de translação - Eixo X e Y.
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
	{
		translateY += TRANSLATE_SPEED;
	}
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
	{
		translateY -= TRANSLATE_SPEED;
	}
	if (key == GLFW_KEY_A && action == GLFW_PRESS)
	{
		translateX -= TRANSLATE_SPEED;
	}
	if (key == GLFW_KEY_D && action == GLFW_PRESS)
	{
		translateX += TRANSLATE_SPEED;
	}

	// Controles de translação - Eixo Z.
	if (key == GLFW_KEY_I && action == GLFW_PRESS)
	{
		translateZ -= TRANSLATE_SPEED;
	}
	if (key == GLFW_KEY_J && action == GLFW_PRESS)
	{
		translateZ += TRANSLATE_SPEED;
	}

	// Controles de escala.
	if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_PRESS)
	{
		scale -= SCALE_SPEED;
		if (scale < 0.1f) scale = 0.1f;
	}
	if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_PRESS)
	{
		scale += SCALE_SPEED;
		if (scale > 3.0f) scale = 3.0f;
	}
}

// Retorna o identificador do programa de shader.
int setupShader()
{
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	GLint success;
	GLchar infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

// Esta função cria os buffers que armazenam a geometria de um cubo.
int setupGeometry()
{
	GLfloat vertices[] = {
		-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
		 
		-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,

		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
		
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
		
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,

		 0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
		
		 0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
		
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
		
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
	};

	GLuint VBO, VAO;

	//Geração do identificador do VBO.
	glGenBuffers(1, &VBO);

	//Faz a conexão (vincula) do buffer como um buffer de array.
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	//Envia os dados do array de floats para o buffer da OpenGl.
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	//Geração do identificador do VAO.
	glGenVertexArrays(1, &VAO);

	// Vincula (bind) o VAO primeiro, e em seguida conecta e seta o(s) buffer(s) de vértices
	// e os ponteiros para os atributos.
	glBindVertexArray(VAO);
	
	//Atributo posição (x, y, z).
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	//Atributo cor (r, g, b).
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	// Desvincula o buffer e o VAO.
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return VAO;
}
