#include <windows.h>
#include "d3d9.h"
#include "d3dx9.h"
#include "Direct3D.h"

// Structure that defines a line
typedef struct {
  D3DXVECTOR3 vecStart;
  D3DXVECTOR3 vecEnd;
} sLine;

// Structure that defines a cubic Bezier curve
typedef struct {
  D3DXVECTOR3 vecStart;
  D3DXVECTOR3 vecEnd;
  D3DXVECTOR3 vecControl1;
  D3DXVECTOR3 vecControl2;
} sCurve;

// Two lines and two curves
sLine  g_Lines[2];
sCurve g_Curves[2];

// Functions to compute coordinates on a line or curve
void Line(sLine *Line, float Scalar, D3DXVECTOR3 *vecOut);
void CubicBezierCurve(sCurve *Curve, float Scalar, D3DXVECTOR3 *vecOut);

// Direct3D objects
IDirect3D9       *g_pD3D       = NULL;
IDirect3DDevice9 *g_pD3DDevice = NULL;

// Robot mesh object
D3DXMESHCONTAINER_EX *g_RobotMesh = NULL;

// Robots' positions and last frame's positions
D3DXVECTOR3 g_vecRobot[4];
D3DXVECTOR3 g_vecRobotLast[4];

// Ground mesh
D3DXMESHCONTAINER_EX *g_GroundMesh = NULL;

// Background vertex structure, fvf, vertex buffer, and texture
typedef struct {
  float x, y, z, rhw;
  float u, v;
} sBackdropVertex;
#define BACKDROPFVF (D3DFVF_XYZRHW | D3DFVF_TEX1)
IDirect3DVertexBuffer9 *g_BackdropVB = NULL;
IDirect3DTexture9      *g_BackdropTexture = NULL;

// Window class and caption text
char g_szClass[]   = "TimedMovementClass";
char g_szCaption[] = "Timed Movement Demo by Jim Adams";

// Function prototypes
int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int nCmdShow);
long FAR PASCAL WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL DoInit(HWND hWnd);
void DoShutdown();
void DoFrame();

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int nCmdShow)
{
  WNDCLASSEX wcex;
  MSG        Msg;
  HWND       hWnd;

  // Initialize the COM system
  CoInitialize(NULL);

  // Create the window class here and register it
  wcex.cbSize        = sizeof(wcex);
  wcex.style         = CS_CLASSDC;
  wcex.lpfnWndProc   = WindowProc;
  wcex.cbClsExtra    = 0;
  wcex.cbWndExtra    = 0;
  wcex.hInstance     = hInst;
  wcex.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
  wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = NULL;
  wcex.lpszMenuName  = NULL;
  wcex.lpszClassName = g_szClass;
  wcex.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
  if(!RegisterClassEx(&wcex))
    return FALSE;

  // Create the main window
  hWnd = CreateWindow(g_szClass, g_szCaption,
              WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
              0, 0, 640, 480,
              NULL, NULL, hInst, NULL);
  if(!hWnd)
    return FALSE;
  ShowWindow(hWnd, SW_NORMAL);
  UpdateWindow(hWnd);

  // Call init function and enter message pump
  if(DoInit(hWnd) == TRUE) {

    // Start message pump, waiting for user to exit
    ZeroMemory(&Msg, sizeof(MSG));
    while(Msg.message != WM_QUIT) {
      if(PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
      }

      // Render a single frame
      DoFrame();
    }
  }

  // Call shutdown
  DoShutdown();
 
  // Unregister the window class
  UnregisterClass(g_szClass, hInst);

  // Shut down the COM system
  CoUninitialize();

  return 0;
}


long FAR PASCAL WindowProc(HWND hWnd, UINT uMsg,              \
                           WPARAM wParam, LPARAM lParam)
{
  // Only handle window destruction messages
  switch(uMsg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    default:
      return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }

  return 0;
}

BOOL DoInit(HWND hWnd)
{
  // Initialize Direct3D
  InitD3D(&g_pD3D, &g_pD3DDevice, hWnd);

  // Load the robot mesh
  LoadMesh(&g_RobotMesh, g_pD3DDevice, "..\\Data\\robot.x", "..\\Data\\");

  // Load the ground mesh
  LoadMesh(&g_GroundMesh, g_pD3DDevice, "..\\Data\\ground.x", "..\\Data\\");

  // Create the backdrop
  sBackdropVertex BackdropVerts[4] = {
    {   0.0f,   0.0, 1.0, 1.0f, 0.0f, 0.0f },
    { 640.0f,   0.0, 1.0, 1.0f, 1.0f, 0.0f },
    {   0.0f, 480.0, 1.0, 1.0f, 0.0f, 1.0f },
    { 640.0f, 480.0, 1.0, 1.0f, 1.0f, 1.0f }
  };
  g_pD3DDevice->CreateVertexBuffer(sizeof(BackdropVerts), D3DUSAGE_WRITEONLY, BACKDROPFVF, D3DPOOL_DEFAULT, &g_BackdropVB, NULL);
  char *Ptr;
  g_BackdropVB->Lock(0,0, (void**)&Ptr, 0);
  memcpy(Ptr, BackdropVerts, sizeof(BackdropVerts));
  g_BackdropVB->Unlock();
  D3DXCreateTextureFromFile(g_pD3DDevice, "..\\Data\\Backdrop.bmp", &g_BackdropTexture);

  // Setup a directional light
  D3DLIGHT9 Light;
  ZeroMemory(&Light, sizeof(D3DLIGHT9));
  Light.Type = D3DLIGHT_DIRECTIONAL;
  Light.Diffuse.r = Light.Diffuse.g = Light.Diffuse.b = Light.Diffuse.a = 1.0f;
  Light.Direction = D3DXVECTOR3(0.0f, -0.5f, 0.5f);
  g_pD3DDevice->SetLight(0, &Light);
  g_pD3DDevice->LightEnable(0, TRUE);

  // Define two lines
  g_Lines[0].vecStart = D3DXVECTOR3(-150.0f, 10.0f,   0.0f);
  g_Lines[0].vecEnd   = D3DXVECTOR3(   0.0f, 10.0f, 150.0f);
  
  g_Lines[1].vecStart = D3DXVECTOR3(0.0f,  10.0f, 0.0f);
  g_Lines[1].vecEnd   = D3DXVECTOR3(0.0f, 150.0f, 0.0f);

  // Define two curves
  g_Curves[0].vecStart    = D3DXVECTOR3(  0.0f, 10.0f, 150.0f);
  g_Curves[0].vecControl1 = D3DXVECTOR3(150.0f, 10.0f, 100.0f);
  g_Curves[0].vecControl2 = D3DXVECTOR3(200.0f, 10.0f,  50.0f);
  g_Curves[0].vecEnd      = D3DXVECTOR3(150.0f, 10.0f,   0.0f);

  g_Curves[1].vecStart    = D3DXVECTOR3(-150.0f, 50.0f, -100.0f);
  g_Curves[1].vecControl1 = D3DXVECTOR3( -20.0f, 0.0f, -100.0f);
  g_Curves[1].vecControl2 = D3DXVECTOR3(  20.0f, 0.0f, -100.0f);
  g_Curves[1].vecEnd      = D3DXVECTOR3( 150.0f, 50.0f, -100.0f);

  return TRUE;
}

void DoShutdown()
{
  // Free mesh data
  delete g_RobotMesh;  g_RobotMesh  = NULL;
  delete g_GroundMesh; g_GroundMesh = NULL;

  // Release Backdrop data
  ReleaseCOM(g_BackdropVB);
  ReleaseCOM(g_BackdropTexture);

  // Release D3D objects
  ReleaseCOM(g_pD3DDevice);
  ReleaseCOM(g_pD3D);
}

void DoFrame()
{
  // Compute a time scalar based on a sine wave
  float Time   = (float)(timeGetTime()) * 0.001f;
  float Scalar = ((float)sin(Time) + 1.0f) * 0.5f;

  // Update the positions of the robots
  Line(&g_Lines[0], Scalar, &g_vecRobot[0]);
  Line(&g_Lines[1], Scalar, &g_vecRobot[1]);
  CubicBezierCurve(&g_Curves[0], Scalar, &g_vecRobot[2]);
  CubicBezierCurve(&g_Curves[1], Scalar, &g_vecRobot[3]);

  // Set a view transformation matrix
  D3DXMATRIX matView;
  D3DXMatrixLookAtLH(&matView,
                     &D3DXVECTOR3(0.0f, 120.0f, -350.0f),
                     &D3DXVECTOR3(0.0f, 0.0f, 0.0f),
                     &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
  g_pD3DDevice->SetTransform(D3DTS_VIEW, &matView);

  // Clear the device and start drawing the scene
  g_pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0,0,64,255), 1.0f, 0);
  if(SUCCEEDED(g_pD3DDevice->BeginScene())) {

    // Draw the backdrop
    g_pD3DDevice->SetFVF(BACKDROPFVF);
    g_pD3DDevice->SetStreamSource(0, g_BackdropVB, 0, sizeof(sBackdropVertex));
    g_pD3DDevice->SetTexture(0, g_BackdropTexture);
    g_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    // Enable lighting
    g_pD3DDevice->SetRenderState(D3DRS_LIGHTING, TRUE);

    // Draw the ground mesh
    D3DXMATRIX matWorld;
    D3DXMatrixIdentity(&matWorld);
    g_pD3DDevice->SetTransform(D3DTS_WORLD, &matWorld);
    DrawMesh(g_GroundMesh);

    // Draw each of the four robots at their respective locations
    for(DWORD i=0;i<4;i++) {

      // Calculate the rotation of the robots based on last
      // known position, and update last position once done.
      D3DXVECTOR3 vecDiff = g_vecRobot[i] - g_vecRobotLast[i];
      float RotX =  (float)atan2(vecDiff.y, vecDiff.z);
      float RotY = -(float)atan2(vecDiff.z, vecDiff.x);
      g_vecRobotLast[i] = g_vecRobot[i];

      // Rotate the robot to point in direction of movement
      D3DXMatrixRotationYawPitchRoll(&matWorld, RotY, RotX, 0.0f);

      // Position the robot by setting the coordinates
      // directly in the world transformation matrix
      matWorld._41 = g_vecRobot[i].x;
      matWorld._42 = g_vecRobot[i].y;
      matWorld._43 = g_vecRobot[i].z;
      g_pD3DDevice->SetTransform(D3DTS_WORLD, &matWorld);

      // Draw the robot mesh
      DrawMesh(g_RobotMesh);
    }

    // Disable lighting
    g_pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // End the scene
    g_pD3DDevice->EndScene();
  }

  // Present the scene to the user
  g_pD3DDevice->Present(NULL, NULL, NULL, NULL);
}

void Line(sLine *Line, float Scalar, D3DXVECTOR3 *vecOut)
{
  // Get the length of the path
  float Length = D3DXVec3Length(&(Line->vecEnd - Line->vecStart));

  // Use scalar to calculate coordinates
  (*vecOut) = (Line->vecEnd - Line->vecStart) * Scalar + Line->vecStart;
}

void CubicBezierCurve(sCurve *Curve, float Scalar, D3DXVECTOR3 *vecOut)
{
  *vecOut =
    (Curve->vecStart)*(1.0f-Scalar)*(1.0f-Scalar)*(1.0f-Scalar)  +
    (Curve->vecControl1)*3.0f*Scalar*(1.0f-Scalar)*(1.0f-Scalar) +
    (Curve->vecControl2)*3.0f*Scalar*Scalar*(1.0f-Scalar)        +
    (Curve->vecEnd)*Scalar*Scalar*Scalar;
}
