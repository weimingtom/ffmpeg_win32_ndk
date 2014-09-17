#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <math.h>
#include <gl/gl.h>
#include <gl/glu.h>	
#include <GL/glut.h>
#include "TextureLoader.h"

#define FILENAME "image1.bmp"
#define TITLE "OpenGL & GLUT sample program"

UINT g_TextureArray[1];
int g_ID = 0;
unsigned int g_TextureWidth = 100;
unsigned int g_TextureHeight = 100;

void display(void)
{
	float scale = 1.0f;
	float z = 0.0f;
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0.0f, 0.0f, 0.0f);
	
	glBindTexture(GL_TEXTURE_2D, g_TextureArray[0]);
	
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(-1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, 1.0f, 0.0f);
	glEnd();

	glutSwapBuffers();
}

void reshape(int w, int h)
{
	if (h == 0)
	{
		h = 1;
	}
	glViewport(0, 0, w, h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	//FIXME:
	//gluPerspective(45.0f, (GLfloat)w / (GLfloat)h, 0.1f, 50.0f);

	//gluOrtho2D(-1.0f, 1.0f, -1.0f, 1.0f);
	glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void mouse(int button, int state, int mx, int my)
{
}

void motion(int mx, int my)
{
}

void idle(void)
{
}

int initEnvironment(void)
{	
	unsigned char * data;
	
	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_TEXTURE_2D);

	data = loadBMPRaw(FILENAME, &g_TextureWidth, &g_TextureHeight, 0, 1);	
	if (data)
	{
		glGenTextures(1, &g_TextureArray[g_ID]);
		glBindTexture(GL_TEXTURE_2D, g_TextureArray[g_ID]);
		//gluBuild2DMipmaps(GL_TEXTURE_2D, 3, g_TextureWidth, g_TextureHeight, GL_RGB, GL_UNSIGNED_BYTE, data);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_TextureWidth, g_TextureHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		free(data);
		return 1;
	}
	else
	{
		return(0);
	}
}

int main(int argc, char ** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
	glutInitWindowSize(g_TextureWidth, g_TextureHeight);
	glutInitWindowPosition(0, 0);
	glutCreateWindow(TITLE);

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutIdleFunc(idle);

	if (initEnvironment())
	{
		glutMainLoop();
	}

	return 0;
}

