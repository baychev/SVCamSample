
#include "stdafx.h"
#include "afxdialogex.h"
#include <string>
#include <chrono>
#include "SVCamSample.h"
#include "SVCamSampleDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)

END_MESSAGE_MAP()


UINT  DisplayThreadfunction(LPVOID pParamt)

{
	// Process only the currently selected camera.
	CSVCamSampleDlg  *svCam = (CSVCamSampleDlg*)pParamt;
	if (svCam == NULL)
	{
		delete svCam;
		return 0;
	}
	if (svCam->currentSelected_Camera == NULL)
		return 0;

	Camera *  currentCam = svCam->currentSelected_Camera;
	while (!svCam->terminated)
	{
		if (currentCam->sv_cam_acq->imageBufferInfo.size() != 0)
		{
			// Obtain the image data pointer and characteristics
			SV_BUFFER_INFO  *NewImageInfo = currentCam->sv_cam_acq->imageBufferInfo.front();

			if (NewImageInfo->pImagePtr) {
				svCam->PostMessage(WM_DISPLAY_IMAGE, 0, (LONG_PTR)NewImageInfo);
				currentCam->sv_cam_acq->imageBufferInfo.pop_front();
			}
			else
			{
				currentCam->sv_cam_acq->imageBufferInfo.pop_front();
				delete NewImageInfo;
			}
		}
		else
		{
			WaitForSingleObject(currentCam->sv_cam_acq->m_newImage, 1000);
			ResetEvent(currentCam->sv_cam_acq->m_newImage);
		}
	}


	SetEvent(svCam->m_acquisitionstopThread);

	return 0;
}


CSVCamSampleDlg::CSVCamSampleDlg(CWnd* pParent /*=NULL*/) : CDialogEx(CSVCamSampleDlg::IDD, pParent)
{
	//Initalize a camera container for each of GenTLProducers.
	usbCam = NULL;
	gigeCam = NULL;
	clCam = NULL;

	currentSelected_Camera = NULL;
	selectedItem = NULL;
	currentFeatureVisibility = SV_Beginner;

	m_acquisitionstopThread = CreateEvent(NULL, false, false, NULL);
	m_thread = NULL;

	//Initialize image Data for Display
	ImageData_RGB = NULL;
	ImageData_MONO = NULL;

	m_hIcon = AfxGetApp()->LoadIcon(IDI_ICON1);
}

CSVCamSampleDlg::~CSVCamSampleDlg()
{
	// Close the camera container for each of GenTLProducers.
	if (usbCam)
	{
		delete usbCam;
		usbCam = NULL;
	}
	if (gigeCam)
	{
		delete gigeCam;
		gigeCam = NULL;
	}
	if (clCam)
	{
		delete clCam;
		clCam = NULL;
	}

	if (currentSelected_Camera)
		currentSelected_Camera = NULL;

	//Close the library and free all the allocated resources.
	SVLibClose();

	if (display_Data)
	{
		GlobalFree(display_Data);
		ImageData_MONO = NULL;
	}
}

Camera * CSVCamSampleDlg::GetCamera(SVCamSystem* svcam, char* id)
{

	// Get the Camera with the selected device id
	for (std::vector<Camera*>::iterator currentcam = svcam->sv_cam_list.begin(); currentcam != svcam->sv_cam_list.end(); currentcam++)
	{
		std::string str((*currentcam)->devInfo.uid);
		if (str.compare(id) == 0)
		{
			return  (*currentcam);
		}
	}
	return NULL;
}

bool CSVCamSampleDlg::OpenSelectedCam(SVCamSystem* svcam, size_t CamIndex, size_t buffercount)
{

	BeginWaitCursor();
	// Get the Device info with the selected index
	std::vector<SV_DEVICE_INFO*>::iterator devinf = svcam->devInfoList.begin();
	devinf += CamIndex;
	SV_DEVICE_INFO  *devInfo = (*devinf);

	//Open the currently selected device and add it to the list of the opened cameras. Each camera has its own ID
	svcam->openDevice(*devInfo);

	Camera * cam = GetCamera(svcam, devInfo->uid);

	if (cam == NULL)
		return false;

	if (cam)
	{
		// clear display
		ShowImage(Display, 600, 600, display_Data);

		//update the current slected camera
		currentSelected_Camera = cam;

		// Update the feature tree of the device
		UpdateFeatureTree();

	}

	m_start_stream.ShowWindow(SW_SHOWNORMAL);
	m_stop_stream.ShowWindow(SW_HIDE);

	EndWaitCursor();
	return true;
}


LRESULT CSVCamSampleDlg::WMDisplayImage(WPARAM WParam, LPARAM LParam)
{
	// Obtain image information structure
	SV_BUFFER_INFO *   ImageInfo = (SV_BUFFER_INFO *)LParam;
	if (ImageInfo == NULL)
		return 0;

	if (ImageInfo->pImagePtr == NULL)
		return 0;

	// update the displayed image id
	const CString  strValue(to_string(ImageInfo->iImageId).c_str());
	LPCWSTR wstrValue = static_cast<LPCWSTR>(strValue);
	m_frame_id.SetWindowTextW(wstrValue);

	// Check if a RGB image( Bayer buffer format) arrived
	bool isImgRGB = false;
	int pDestLength = (int)(ImageInfo->iImageSize);



	//  Bayer buffer format(up id: 8)
	if ((ImageInfo->iPixelType & SV_GVSP_PIX_ID_MASK) >= 8)
	{
		isImgRGB = true;
		pDestLength = 3 * pDestLength;
	}


	unsigned sizeX = ImageInfo->iSizeX;

	unsigned sizeY = ImageInfo->iSizeY;


	// 8 bit Format
	if ((ImageInfo->iPixelType & SV_GVSP_PIX_EFFECTIVE_PIXELSIZE_MASK) == SV_GVSP_PIX_OCCUPY8BIT)
	{
		if (isImgRGB)
		{
			// Allocate a RGB buffer if needed
			if (NULL == ImageData_RGB)
				ImageData_RGB = (unsigned char *)GlobalAlloc(GMEM_FIXED, pDestLength);


			// Convert to 24 bit and display image
			SVUtilBufferBayerToRGB(*ImageInfo, ImageData_RGB, pDestLength);

			// NOTE: uncomment to save images
			SaveImage(sizeX, sizeY, ImageData_RGB);
			
			ShowImageRGB(Display, sizeX, sizeY, ImageData_RGB);
		}
		else
		{
			ShowImage(Display, sizeX, sizeY, ImageInfo->pImagePtr);
		}
	}


	// 12 bit Format
	// Check if a conversion of a 12-bit image is needed
	if ((ImageInfo->iPixelType & SV_GVSP_PIX_EFFECTIVE_PIXELSIZE_MASK) == SV_GVSP_PIX_OCCUPY12BIT)
	{

		if (isImgRGB)
		{
			// Allocate a RGB buffer if needed
			if (NULL == ImageData_RGB)
				ImageData_RGB = (unsigned char *)GlobalAlloc(GMEM_FIXED, pDestLength);


			// Convert to 24 bit and display image
			SVUtilBufferBayerToRGB(*ImageInfo, ImageData_RGB, pDestLength);


			ShowImageRGB(Display, sizeX, sizeY, ImageData_RGB);
		}
		else
		{
			if (NULL == ImageData_MONO)
				ImageData_MONO = (unsigned char *)GlobalAlloc(GMEM_FIXED, pDestLength);

			// Convert to 8 bit and display image
			SVUtilBuffer12BitTo8Bit(*ImageInfo, ImageData_MONO, pDestLength);
			ShowImage(Display, sizeX, sizeY, ImageData_MONO);
		}
	}


	// 16 bit Format
	// Check if a conversion of a 16-bit image is needed
	if ((ImageInfo->iPixelType & SV_GVSP_PIX_EFFECTIVE_PIXELSIZE_MASK) == SV_GVSP_PIX_OCCUPY16BIT)
	{

		if (isImgRGB)
		{

			// Allocate a RGB buffer if needed
			if (NULL == ImageData_RGB)
				ImageData_RGB = (unsigned char *)GlobalAlloc(GMEM_FIXED, pDestLength);

			// Convert to 24 bit and display image
			SVUtilBuffer16BitTo8Bit(*ImageInfo, ImageData_RGB, (int)pDestLength);
			ShowImageRGB(Display, sizeX, sizeY, ImageData_RGB);
		}
		else
		{
			if (NULL == ImageData_MONO)
				ImageData_MONO = (unsigned char *)GlobalAlloc(GMEM_FIXED, pDestLength);

			// Convert to 8 bit and display image
			SVUtilBuffer16BitTo8Bit(*ImageInfo, ImageData_MONO, (int)pDestLength);
			ShowImage(Display, sizeX, sizeY, ImageData_MONO);
		}
	}

	delete ImageInfo;
	return 0;
}

void CSVCamSampleDlg::ShowImage(HDC DisplayDC, size_t _Width, size_t _Height, unsigned char *ImageData)
{

	int Width = (int)_Width;
	int Height = (int)_Height;

	// Check image alignment
	if (Width % 4 == 0)
	{
		BITMAPINFO *bitmapinfo;

		// Generate and fill a bitmap info structure
		bitmapinfo = (BITMAPINFO *)new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];

		bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapinfo->bmiHeader.biWidth = (long)Width;
		bitmapinfo->bmiHeader.biHeight = -Height;
		bitmapinfo->bmiHeader.biBitCount = 8;
		bitmapinfo->bmiHeader.biPlanes = 1;
		bitmapinfo->bmiHeader.biClrUsed = 0;
		bitmapinfo->bmiHeader.biClrImportant = 0;
		bitmapinfo->bmiHeader.biCompression = BI_RGB;
		bitmapinfo->bmiHeader.biSizeImage = 0;
		bitmapinfo->bmiHeader.biXPelsPerMeter = 0;
		bitmapinfo->bmiHeader.biYPelsPerMeter = 0;

		// Fill color table with gray levels
		for (int i = 0; i < 256; i++)
		{
			bitmapinfo->bmiColors[i].rgbRed = i;
			bitmapinfo->bmiColors[i].rgbGreen = i;
			bitmapinfo->bmiColors[i].rgbBlue = i;
			bitmapinfo->bmiColors[i].rgbReserved = 0;
		}

		// Center image if it is smaller than the screen
		int Left = 0;
		int Top = 0;
		int OffsetX = 0;
		if (Width < DisplayWidth)
			Left = (DisplayWidth - Width) / 2;
		if (Height < DisplayHeight)
			Top = (DisplayHeight - Height) / 2;
		// Center image if it is bigger than the screen
		if (Width > DisplayWidth)
			OffsetX = (Width - DisplayWidth) / 2;
		SetDIBitsToDevice(DisplayDC, Left, Top, Width, Height, OffsetX, 0, 0, Height, ImageData, bitmapinfo, DIB_RGB_COLORS);


		delete[]bitmapinfo;

	}
}

void CSVCamSampleDlg::ShowImageRGB(HDC DisplayDC, size_t _Width, size_t _Height, unsigned char *ImageData)
{

	int Width = (int)_Width;
	int Height = (int)_Height;

	// Check image alignment
	if (Width % 4 == 0)
	{
		BITMAPINFO *bitmapinfo;

		// Generate and fill a bitmap info structure
		bitmapinfo = (BITMAPINFO *)new char[sizeof(BITMAPINFOHEADER)];

		bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapinfo->bmiHeader.biWidth = Width;
		bitmapinfo->bmiHeader.biHeight = -Height;
		bitmapinfo->bmiHeader.biBitCount = 24;
		bitmapinfo->bmiHeader.biPlanes = 1;
		bitmapinfo->bmiHeader.biClrUsed = 0;
		bitmapinfo->bmiHeader.biClrImportant = 0;
		bitmapinfo->bmiHeader.biCompression = BI_RGB;
		bitmapinfo->bmiHeader.biSizeImage = 0;
		bitmapinfo->bmiHeader.biXPelsPerMeter = 0;
		bitmapinfo->bmiHeader.biYPelsPerMeter = 0;

		// Center image if it is smaller than the screen
		int Left = 0;
		int Top = 0;
		int OffsetX = 0;
		if (Width < DisplayWidth)
			Left = (DisplayWidth - Width) / 2;
		if (Height < DisplayHeight)
			Top = (DisplayHeight - Height) / 2;
		// Center image if it is bigger than the screen
		if (Width > DisplayWidth)
			OffsetX = (Width - DisplayWidth) / 2;

		SetDIBitsToDevice(DisplayDC, Left, Top, Width, Height, OffsetX, 0, 0, Height, ImageData, bitmapinfo, DIB_RGB_COLORS);

		delete[]bitmapinfo;

	}
}

void CSVCamSampleDlg::AddFeatureToTree(SVCamFeaturInf* curentfeature, HTREEITEM *root)
{

	TVINSERTSTRUCT tvInsert;
	ZeroMemory(&tvInsert, sizeof(tvInsert));
	tvInsert.hInsertAfter = NULL;
	tvInsert.item.mask = TVIF_TEXT;
	WCHAR szText[512] = { '\0' };
	tvInsert.item.pszText = szText;
	HTREEITEM current_root = NULL;
	HTREEITEM parentroot = NULL;

	//Get the name and it correspond value.
	const CString str(((curentfeature)->SVFeaturInf.displayName));
	LPCWSTR wstr = static_cast<LPCWSTR>(str);
	wcscpy_s(szText, wstr);

	wcscat_s(szText, L": ");

	const CString  strValue(((curentfeature)->strValue));
	LPCWSTR wstrValue = static_cast<LPCWSTR>(strValue);
	wcscat_s(szText, wstrValue);


	if (SV_intfICategory != (curentfeature)->SVFeaturInf.type)
		currentSelected_Camera->sv_cam_feature->RegisterInvalidateCB((curentfeature)->SVFeaturInf.name, reinterpret_cast<SV_CB_OBJECT>(this), reinterpret_cast<SV_CB_FEATURE_INVALIDATED_PFN>(&CSVCamSampleDlg::OnFeatureInvalidated));

	//the features are grouped in levels.
	//Add the feature items depending on it levels
	switch ((curentfeature)->SVFeaturInf.level)
	{

	case 1:
		*root = m_feature_tree.InsertItem(&tvInsert);
		m_feature_tree.SetItemData(*root, ((DWORD_PTR)(curentfeature)));
		m_feature_tree.SelectItem(*root);
		break;

	default:
		//Add the current feature item to the next feature with higher level.
		parentroot = FindItemByLevel((curentfeature)->SVFeaturInf.level - 1, m_feature_tree, m_feature_tree.GetSelectedItem());
		current_root = m_feature_tree.InsertItem(tvInsert.item.pszText, parentroot);

		// disable items with locked features.
		if ((curentfeature)->SVFeaturInf.isLocked)
		{
			m_feature_tree.SetItemStateEx(current_root, TVIS_EX_DISABLED);
		}

		m_feature_tree.SetItemData(current_root, (DWORD_PTR)(curentfeature));

		m_feature_tree.SelectItem(current_root);
		break;
	}
}

void CSVCamSampleDlg::UpdateFeatureTree()
{
	Camera *  cam = currentSelected_Camera;
	if (cam)
	{
		// Update the feature tree of the selected camera
		m_feature_tree.ShowWindow(SW_HIDE);
		m_edit_feature_value.ShowWindow(SW_HIDE);
		m_Feature_value.ShowWindow(SW_HIDE);
		m_command.ShowWindow(SW_HIDE);
		m_enumeration.ShowWindow(SW_HIDE);
		m_string_value.ShowWindow(SW_HIDE);
		m_feature_tree.DeleteAllItems();

		DSDeleteContainer(cam->sv_cam_feature->featureInfolist);
		cam->sv_cam_feature->getDeviceFeatureList(currentFeatureVisibility);


		if (cam->sv_cam_feature->featureInfolist.size() == 0)
			return;

		HTREEITEM root = NULL;

		HTREEITEM root1 = NULL;


		for (std::vector<SVCamFeaturInf*>::iterator i = cam->sv_cam_feature->featureInfolist.begin() + 1; i != cam->sv_cam_feature->featureInfolist.end(); i++)
		{
			AddFeatureToTree(*i, &root);
			m_feature_tree.Expand(root, TVE_COLLAPSE);
			if (root1 == NULL)
				root1 = root;
		}
		// expand the first feature node with camera information.
		m_feature_tree.Expand(root1, TVE_EXPAND);

		currentSelected_Camera->isInvalidateCB = true;

		m_feature_tree.ShowWindow(SW_SHOWNORMAL);
	}
}

HTREEITEM CSVCamSampleDlg::FindItemByName(const CString& name, CTreeCtrl& tree, HTREEITEM hRoot)
{
	// check whether the current item is the searched one
	SVCamFeaturInf *camFeatureInfo = (SVCamFeaturInf *)tree.GetItemData(hRoot);
	CString text(camFeatureInfo->SVFeaturInf.name);

	if (text.Compare(name) == 0)
		return hRoot;

	// get a handle to the first child item
	HTREEITEM hSubItem = tree.GetChildItem(hRoot);

	// iterate as long a new item is found
	while (hSubItem)
	{
		// check the children of the current item
		HTREEITEM hFound = FindItemByName(name, tree, hSubItem);
		if (hFound)
			return hFound;

		// get the next sibling of the current item
		//hSubItem = tree.GetNextSiblingItem(hSubItem);
		hSubItem = tree.GetNextVisibleItem(hSubItem);
	}

	// return NULL if nothing was found
	return NULL;
}

HTREEITEM CSVCamSampleDlg::FindItemByLevel(int Level, CTreeCtrl& tree, HTREEITEM hRoot)
{
	// check whether the current item is the searched one
	SVCamFeaturInf *camFeatureInfo = (SVCamFeaturInf *)tree.GetItemData(hRoot);
	CString text(camFeatureInfo->SVFeaturInf.displayName);

	if (camFeatureInfo->SVFeaturInf.level <= Level)
	{
		return hRoot;
	}
	// get a handle to the first child item
	HTREEITEM hSubItem = tree.GetParentItem(hRoot);
	// iterate as long a new item is found
	while (hSubItem)
	{
		// check the children of the current item
		HTREEITEM hFound = FindItemByLevel(Level, tree, hSubItem);
		if (hFound)
			return hFound;

		// get the next sibling of the current item
		hSubItem = tree.GetPrevSiblingItem(hSubItem);
	}
	// return NULL if nothing was found
	return NULL;
}

void CSVCamSampleDlg::OnFeatureInvalidated(const char *featureName) //HTREEITEM *root)
{

	// get the changed value and update the item value in the tree
	if (currentSelected_Camera)
	{
		const CString  str(featureName);
		HTREEITEM  currentitem = FindItemByName(str, m_feature_tree, m_feature_tree.GetRootItem());
		UpdateTreeItem(currentitem);
	}
}


void CSVCamSampleDlg::UpdateTreeItem(HTREEITEM hRoot)
{

	if (hRoot)
	{
		SVCamFeaturInf * inf = (SVCamFeaturInf *)m_feature_tree.GetItemData(hRoot);

		if (inf == NULL)
			return;
		if (inf->SVFeaturInf.type != SV_intfICommand)
		{
			currentSelected_Camera->sv_cam_feature->getFeatureValue(((SVCamFeaturInf *)m_feature_tree.GetItemData(hRoot))->hFeature, inf);

			WCHAR szText[512] = { '\0' };
			const CString str(inf->SVFeaturInf.displayName);
			LPCWSTR wsstr = static_cast<LPCWSTR>(str);
			wcscat_s(szText, wsstr);
			wcscat_s(szText, L": ");
			const CString str1(inf->strValue);
			LPCWSTR wsstr1 = static_cast<LPCWSTR>(str1);
			wcscat_s(szText, wsstr1);
			// update feature value 
			m_feature_tree.SetItemText(hRoot, szText);
		}

		if (inf->SVFeaturInf.type != SV_intfICategory)
		{
			if ((inf)->SVFeaturInf.isLocked)
			{
				m_feature_tree.SetItemStateEx(hRoot, TVIS_EX_DISABLED);
			}
			else
			{
				m_feature_tree.SetItemStateEx(hRoot, 0);
			}
		}
	}
}

void CSVCamSampleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	//Device
	DDX_Control(pDX, IDC_USB_CAM, m_usb_cameras);
	DDX_Control(pDX, IDC_GigE_CAM, m_gige_cameras);
	DDX_Control(pDX, IDC_CAMERA_LINK, m_cl_cameras);

	//Acquisition(stream)
	DDX_Control(pDX, IDC_BUTTON2, m_stop_stream);
	DDX_Control(pDX, IDC_BUTTON3, m_start_stream);
	DDX_Control(pDX, IDC_DISPLAY, m_display);
	DDX_Control(pDX, IDC_EDIT5, m_frame_id);

	//Device Feature(control)
	DDX_Control(pDX, IDC_TREE2, m_feature_tree);
	DDX_Control(pDX, IDC_BUTTON4, m_command);
	DDX_Control(pDX, IDC_COMBO1, m_enumeration);
	DDX_Control(pDX, IDC_SLIDER1, m_Feature_value);
	DDX_Control(pDX, IDC_EDIT1, m_edit_feature_value);
	DDX_Control(pDX, IDC_EDIT2, m_tool_tip);
	DDX_Control(pDX, IDC_EDIT3, m_current_feature);
	DDX_Control(pDX, IDC_EDIT4, m_string_value);
	DDX_Control(pDX, IDC_BUTTON1, m_update_tree);
	DDX_Control(pDX, IDC_COMBO2, m_feature_visibility);
}

BEGIN_MESSAGE_MAP(CSVCamSampleDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()

	// Device Selection
	ON_CBN_SELCHANGE(IDC_USB_CAM, &CSVCamSampleDlg::OnCbnSelchangeUsbCam)
	ON_CBN_SELCHANGE(IDC_GigE_CAM, &CSVCamSampleDlg::OnCbnSelchangeGigeCam)
	ON_CBN_SELCHANGE(IDC_CAMERA_LINK, &CSVCamSampleDlg::OnCbnSelchangeCameraLink)

	//Acquisition(streamming channel)
	ON_BN_CLICKED(IDC_BUTTON2, &CSVCamSampleDlg::OnBnClickedStopStream)
	ON_BN_CLICKED(IDC_BUTTON3, &CSVCamSampleDlg::OnBnClickedStartStream)
	ON_MESSAGE(WM_DISPLAY_IMAGE, WMDisplayImage)

	//Device Feature(control)
	ON_BN_CLICKED(IDC_BUTTON4, &CSVCamSampleDlg::OnBnClickedFeatureCommand)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SLIDER1, &CSVCamSampleDlg::OnDrawFeatureSlider)
	ON_CBN_SELCHANGE(IDC_COMBO1, &CSVCamSampleDlg::OnSelectEnumFeature)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE2, &CSVCamSampleDlg::OnDblclkFeatureTree)
	ON_EN_KILLFOCUS(IDC_EDIT4, &CSVCamSampleDlg::OnEnEditFeatureString)
	ON_BN_CLICKED(IDC_BUTTON1, &CSVCamSampleDlg::OnBnUpdateFeatureTree)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER1, &CSVCamSampleDlg::OnReleasedcaptureFeatureSlider)
	ON_EN_KILLFOCUS(IDC_EDIT1, &CSVCamSampleDlg::OnEnFeatureEdit)

	//distructor
	ON_BN_CLICKED(IDCLOSE, &CSVCamSampleDlg::OnBnClickedQuit)

	ON_CBN_SELCHANGE(IDC_COMBO2, &CSVCamSampleDlg::OnCbnSelchangeVisibility)
END_MESSAGE_MAP()


BOOL CSVCamSampleDlg::OnInitDialog()
{

	CDialogEx::OnInitDialog();
	SetIcon(m_hIcon, TRUE);


	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}


	// Obtain a device context for displaying bitmaps 
	Display = ::GetDC(m_display);
	// Determine display geometry
	RECT DisplayRect;
	m_display.GetWindowRect(&DisplayRect);
	DisplayWidth = DisplayRect.right - DisplayRect.left;
	DisplayHeight = DisplayRect.bottom - DisplayRect.top;
	display_Data = (unsigned char *)GlobalAlloc(GMEM_FIXED, 600 * 600);
	memset(display_Data, 0, 600 * 600);



	// Initialize device selection
	m_usb_cameras.AddString(L"USB 3.0 vision");
	m_gige_cameras.AddString(L"GigE vision");
	m_cl_cameras.AddString(L"Camera Link");

	m_feature_visibility.AddString(L"Guru");
	m_feature_visibility.AddString(L"Expert");
	m_feature_visibility.AddString(L"Beginner");

	m_feature_visibility.SetCurSel(currentFeatureVisibility);



	m_gige_cameras.SetCurSel(0);
	m_usb_cameras.SetCurSel(0);
	m_cl_cameras.SetCurSel(0);

	char SVGenicamRoot[1024] = { 0 };
	char SVGenicamCache[1024] = { 0 };
	char SVSCti[1024] = { 0 };
	char SVCLProtocol[1024] = { 0 };

	GetEnvironmentVariableA("SVS_GENICAM_ROOT", SVGenicamRoot, sizeof(SVGenicamRoot));
#ifdef _WIN64
	GetEnvironmentVariableA("SVS_SDK_GENTL", SVSCti, sizeof(SVSCti));
#else
	GetEnvironmentVariableA("SVS_SDK_GENTL32", SVSCti, sizeof(SVSCti));
#endif

	GetEnvironmentVariableA("SVS_GENICAM_CLPROTOCOL", SVCLProtocol, sizeof(SVCLProtocol));
	GetEnvironmentVariableA("SVS_GENICAM_CACHE", SVGenicamCache, sizeof(SVCLProtocol));

	SV_RETURN  ret = SVLibInit(SVSCti, SVGenicamRoot, SVGenicamCache, SVCLProtocol);
	if (ret != SV_ERROR_SUCCESS)
	{
		MessageBox(_T(" SVGenSDK could not be initalized"));
		PostMessage(WM_CLOSE, 0, NULL);
		return false;
	}

	gigeCam = new SVCamSystem(TL_GEV);
	usbCam = new SVCamSystem(TL_U3V);
	//clCam    = new SVCamSystem(TL_CL);

	m_command.ShowWindow(SW_HIDE);
	m_Feature_value.ShowWindow(SW_HIDE);

	m_enumeration.Clear();
	for (int j = 0; j < m_enumeration.GetCount() + 1; j++)
		m_enumeration.DeleteString(j);
	m_enumeration.ShowWindow(SW_HIDE);
	m_edit_feature_value.ShowWindow(SW_HIDE);


	//Open the System module for each of GenTLProducers.
	uint32_t tlCount = 0;
	ret = SVLibSystemGetCount(&tlCount);

	bool usbStat = false;
	bool gigeStat = false;
	bool clcamStat = false;

	for (uint32_t i = 0; i < tlCount; i++)
	{
		SV_TL_INFO tlInfo = { 0 };
		ret = SVLibSystemGetInfo(i, &tlInfo);
		if (SV_ERROR_SUCCESS != ret)
		{
			continue;
		}

		if (0 == _stricmp("GEV", tlInfo.tlType))
			gigeStat = gigeCam->SVCamSystemInit(i);

		if (0 == _stricmp("U3V", tlInfo.tlType))
			usbStat = usbCam->SVCamSystemInit(i);


		//if(0 == _stricmp("CL", tlInfo.tlType))
		//clcamStat = clCam->SVCamSystemInit( i);
	}


	// Fill combo box with all available cameras

	if (usbStat)
	{
		usbCam->EnumDevices(1000);
		for (std::vector<SV_DEVICE_INFO*>::iterator i = usbCam->devInfoList.begin(); i != usbCam->devInfoList.end(); ++i)
		{
			const CString  strValue((*i)->model);
			LPCWSTR ws = static_cast<LPCWSTR>(strValue);
			m_usb_cameras.AddString(ws);
		}

	}

	if (gigeStat)
	{
		gigeCam->EnumDevices(1000);
		for (std::vector<SV_DEVICE_INFO*>::iterator i = gigeCam->devInfoList.begin(); i != gigeCam->devInfoList.end(); ++i)
		{
			const CString  strValue((*i)->model);
			LPCWSTR ws = static_cast<LPCWSTR>(strValue);
			m_gige_cameras.AddString(ws);
		}
	}
	/*
	if(clcamStat)
	{
		  // cam link may take time for the initalisation
			clCam->EnumDevices(30000);
			 for(std::vector<SV_DEVICE_INFO*>::iterator i =  clCam->devInfoList.begin(); i !=  clCam->devInfoList.end(); ++i)
		 {
			 const CString  strValue( (*i)->model);
				LPCWSTR ws = static_cast<LPCWSTR>( strValue );

				m_cl_cameras.AddString(ws);
		 }
	}
	*/

	ShowWindow(SW_SHOWNORMAL);
	return TRUE;
}

void CSVCamSampleDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}

	if ((nID & 0xFFF0) == SC_CLOSE)
	{
		OnBnClickedQuit();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CSVCamSampleDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CSVCamSampleDlg::OnBnClickedFeatureCommand()

{
	UINT32 timeOut = 1000;
	DWORD_PTR data = m_feature_tree.GetItemData(selectedItem);
	SVCamFeaturInf *currentSelected_Featur = ((SVCamFeaturInf *)data);
	SVFeatureCommandExecute(currentSelected_Featur->hRemoteDevice, currentSelected_Featur->hFeature, timeOut);
}

void CSVCamSampleDlg::OnDrawFeatureSlider(NMHDR *pNMHDR, LRESULT *pResult)
{
	const CString  strValue(to_string(m_Feature_value.GetPos()).c_str());
	LPCWSTR ws = static_cast<LPCWSTR>(strValue);
	m_edit_feature_value.SetWindowTextW(ws);
	*pResult = 0;
}

void CSVCamSampleDlg::OnEnFeatureEdit()
{


	if (m_feature_tree)
	{
		const int size = 255;
		TCHAR buffer[size] = { 0 };
		m_edit_feature_value.GetWindowText(buffer, size);

		int64_t  intValue = _wtol(buffer);


		if (m_Feature_value.GetRangeMax() >= 0)
		{
			if (intValue > m_Feature_value.GetRangeMax())
				intValue = m_Feature_value.GetRangeMax();
			m_Feature_value.SetPos((int)intValue);
		}


		if (selectedItem)
		{

			DWORD_PTR data = m_feature_tree.GetItemData(selectedItem);
			SVCamFeaturInf *currentSelected_Featur = ((SVCamFeaturInf *)data);


			if (currentSelected_Featur)
				switch (currentSelected_Featur->SVFeaturInf.type)
				{
				case SV_intfIInteger:
					if (intValue > currentSelected_Featur->SVFeaturInf.intMax)
						intValue = currentSelected_Featur->SVFeaturInf.intMax;

					intValue = intValue - intValue % currentSelected_Featur->SVFeaturInf.intInc;
					SVFeatureSetValueInt64(currentSelected_Featur->hRemoteDevice, currentSelected_Featur->hFeature, intValue);
					break;

				case SV_intfIFloat:
					if (intValue > (int64_t)currentSelected_Featur->SVFeaturInf.floatMax)
						intValue = (int64_t)currentSelected_Featur->SVFeaturInf.floatMax;

					intValue = intValue - intValue % (int64_t)currentSelected_Featur->SVFeaturInf.floatInc;
					SVFeatureSetValueFloat(currentSelected_Featur->hRemoteDevice, currentSelected_Featur->hFeature, (double)intValue);
					break;

				default:
					return;
				}

			const CString  strValue(to_string(intValue).c_str());
			LPCWSTR ws = static_cast<LPCWSTR>(strValue);
			m_edit_feature_value.SetWindowTextW(ws);
			UpdateTreeItem(selectedItem);
		}
	}
}

void CSVCamSampleDlg::OnReleasedcaptureFeatureSlider(NMHDR *pNMHDR, LRESULT *pResult)
{

	DWORD_PTR data = m_feature_tree.GetItemData(selectedItem);
	SVCamFeaturInf *currentSelected_Featur = ((SVCamFeaturInf *)data);

	int64_t  value = m_Feature_value.GetPos();

	switch (currentSelected_Featur->SVFeaturInf.type)
	{
	case SV_intfIInteger:
		value = value - value % (int)currentSelected_Featur->SVFeaturInf.intInc;
		m_Feature_value.SetPos((int)value);
		SVFeatureSetValueInt64(currentSelected_Featur->hRemoteDevice, currentSelected_Featur->hFeature, m_Feature_value.GetPos());
		break;

	case SV_intfIFloat:
		value = value - value % (int)currentSelected_Featur->SVFeaturInf.floatInc;
		m_Feature_value.SetPos((int)value);
		SVFeatureSetValueFloat(currentSelected_Featur->hRemoteDevice, currentSelected_Featur->hFeature, m_Feature_value.GetPos());
		break;

	default:
		*pResult = 0;
		return;
	}
	UpdateTreeItem(selectedItem);
	*pResult = 0;
}

void CSVCamSampleDlg::OnSelectEnumFeature()
{

	int32_t enumvalue = m_enumeration.GetCurSel();
	DWORD_PTR data = m_feature_tree.GetItemData(selectedItem);
	SVCamFeaturInf *currentSelected_FeatureInfo = currentSelected_FeatureInfo = (SVCamFeaturInf *)data;

	if (SV_intfIBoolean == currentSelected_FeatureInfo->SVFeaturInf.type)
	{
		bool  bvalue = (enumvalue == 0 ? false : true);
		int ret = SVFeatureSetValueBool(currentSelected_FeatureInfo->hRemoteDevice, currentSelected_FeatureInfo->hFeature, bvalue);
		UpdateTreeItem(selectedItem);
	}
	else
	{

		char *subFeatureName = new char[SV_STRING_SIZE];
		int64_t pValue = 0;
		SVFeatureEnumSubFeatures(currentSelected_FeatureInfo->hRemoteDevice, currentSelected_FeatureInfo->hFeature, enumvalue, subFeatureName, SV_STRING_SIZE, &pValue);
		int ret = SVFeatureSetValueInt64Enum(currentSelected_FeatureInfo->hRemoteDevice, currentSelected_FeatureInfo->hFeature, pValue);
		delete[] subFeatureName;
		UpdateTreeItem(selectedItem);
	}
}

void CSVCamSampleDlg::OnDblclkFeatureTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (m_feature_tree.GetItemStateEx(m_feature_tree.GetSelectedItem()) == TVIS_EX_DISABLED)
		return;

	selectedItem = m_feature_tree.GetSelectedItem();
	DWORD_PTR data = m_feature_tree.GetItemData(selectedItem);
	SVCamFeaturInf *currentSelected_FeatureInfo;
	currentSelected_FeatureInfo = (SVCamFeaturInf *)data;
	const CString s((currentSelected_FeatureInfo->SVFeaturInf.toolTip));
	LPCWSTR ws = static_cast<LPCWSTR>(s);
	m_tool_tip.SetWindowTextW(ws);
	const CString  strValue(currentSelected_FeatureInfo->SVFeaturInf.displayName);
	LPCWSTR ws3 = static_cast<LPCWSTR>(strValue);

	m_current_feature.SetWindowTextW(ws3);
	CFont m_Font;
	m_Font.CreatePointFont(100, _T("Calibri"));
	m_current_feature.SetFont(&m_Font, 1);
	m_edit_feature_value.ShowWindow(SW_HIDE);
	m_Feature_value.ShowWindow(SW_HIDE);
	m_command.ShowWindow(SW_HIDE);
	m_enumeration.ShowWindow(SW_HIDE);
	m_enumeration.ResetContent();
	m_string_value.ShowWindow(SW_HIDE);

	if (currentSelected_FeatureInfo)
	{

		if (SV_intfIInteger == currentSelected_FeatureInfo->SVFeaturInf.type || SV_intfIFloat == currentSelected_FeatureInfo->SVFeaturInf.type)
		{
			if (SV_intfIInteger == currentSelected_FeatureInfo->SVFeaturInf.type)
			{


				m_Feature_value.SetRangeMin((int)currentSelected_FeatureInfo->SVFeaturInf.intMin);
				m_Feature_value.SetRangeMax((int)currentSelected_FeatureInfo->SVFeaturInf.intMax);


				m_Feature_value.SetPos((int)currentSelected_FeatureInfo->intValue);

				const CString  strValue(to_string(currentSelected_FeatureInfo->intValue).c_str());
				LPCWSTR ws3 = static_cast<LPCWSTR>(strValue);
				m_edit_feature_value.SetWindowTextW(strValue);
			}

			if (SV_intfIFloat == currentSelected_FeatureInfo->SVFeaturInf.type)
			{
				m_Feature_value.SetRangeMin((int)currentSelected_FeatureInfo->SVFeaturInf.floatMin);
				m_Feature_value.SetRangeMax((int)currentSelected_FeatureInfo->SVFeaturInf.floatMax);
				m_Feature_value.SetPos((int)currentSelected_FeatureInfo->doubleValue);

				const CString  strValue(to_string(currentSelected_FeatureInfo->doubleValue).c_str());
				LPCWSTR ws3 = static_cast<LPCWSTR>(strValue);
				m_edit_feature_value.SetWindowTextW(strValue);
			}

			if (m_Feature_value.GetRangeMax() >= 0)
			{
				m_Feature_value.ShowWindow(SW_NORMAL);
			}
			m_edit_feature_value.ShowWindow(SW_NORMAL);
		}


		if (SV_intfIString == currentSelected_FeatureInfo->SVFeaturInf.type)
		{
			const CString s((currentSelected_FeatureInfo->strValue));
			LPCWSTR ws = static_cast<LPCWSTR>(s);

			m_string_value.Clear();
			m_string_value.SetWindowTextW(ws);
			m_string_value.ShowWindow(SW_NORMAL);
		}

		if (SV_intfICommand == currentSelected_FeatureInfo->SVFeaturInf.type)
		{
			const CString  strValue(currentSelected_FeatureInfo->SVFeaturInf.name);
			LPCWSTR ws3 = static_cast<LPCWSTR>(strValue);
			m_command.SetWindowTextW(ws3);
			m_command.ShowWindow(SW_NORMAL);
		}

		if (SV_intfIEnumeration == currentSelected_FeatureInfo->SVFeaturInf.type)
		{
			for (int j = 0; j < currentSelected_FeatureInfo->SVFeaturInf.enumCount; j++)
			{
				char subFeatureName[SV_STRING_SIZE] = { 0 };
				int ret = SVFeatureEnumSubFeatures(currentSelected_FeatureInfo->hRemoteDevice, currentSelected_FeatureInfo->hFeature, j, subFeatureName, SV_STRING_SIZE);

				const CString  strValue(subFeatureName);
				LPCWSTR ws3 = static_cast<LPCWSTR>(strValue);
				m_enumeration.AddString(ws3);

				if (j == currentSelected_FeatureInfo->SVFeaturInf.enumSelectedIndex)
					m_enumeration.SetCurSel(j);
			}

			m_enumeration.ShowWindow(SW_NORMAL);
		}

		if (SV_intfIBoolean == currentSelected_FeatureInfo->SVFeaturInf.type)
		{
			m_enumeration.AddString(L"False");
			m_enumeration.AddString(L"True");

			if (currentSelected_FeatureInfo->booValue)
				m_enumeration.SetCurSel(1);
			else
				m_enumeration.SetCurSel(0);

			m_enumeration.ShowWindow(SW_NORMAL);
		}
	}

	*pResult = 0;
}

void CSVCamSampleDlg::OnCbnSelchangeUsbCam()
{
	size_t  bufcount = 4;
	int sl = m_usb_cameras.GetCurSel();
	if (sl == 0)
		return;


	//Stop the streaming of the selected camera
	OnBnClickedStopStream();

	if (OpenSelectedCam(usbCam, sl - 1, bufcount))
	{
		m_gige_cameras.SetCurSel(0);
		m_cl_cameras.SetCurSel(0);

		GetDlgItem(m_stop_stream.GetDlgCtrlID())->EnableWindow(TRUE);
		GetDlgItem(m_start_stream.GetDlgCtrlID())->EnableWindow(TRUE);
	}
	else
		m_usb_cameras.SetCurSel(0);

}

void CSVCamSampleDlg::OnCbnSelchangeGigeCam()
{

	size_t  bufcount = 4;
	int sl = m_gige_cameras.GetCurSel();

	if (sl == 0)
		return;

	//Close the streaming  of the selected camera
	OnBnClickedStopStream();

	if (OpenSelectedCam(gigeCam, sl - 1, bufcount))
	{
		m_usb_cameras.SetCurSel(0);
		m_cl_cameras.SetCurSel(0);

		GetDlgItem(m_stop_stream.GetDlgCtrlID())->EnableWindow(TRUE);
		GetDlgItem(m_start_stream.GetDlgCtrlID())->EnableWindow(TRUE);
	}
	else
		m_gige_cameras.SetCurSel(0);

}

void CSVCamSampleDlg::OnCbnSelchangeCameraLink()
{
	/* not yet implemented

	int sl =    m_cl_cameras.GetCurSel();
	if (sl ==0)
	return ;


	size_t  bufcount =  4;
	if (OpenSelectedCam(clCam,  sl-1  ,bufcount))
	{
		m_usb_cameras.SetCurSel(0);m_gige_cameras.SetCurSel(0);

	}
	else
	m_cl_cameras.SetCurSel(0);

	GetDlgItem(m_stop_stream.GetDlgCtrlID())->EnableWindow(FALSE);
	GetDlgItem(m_start_stream.GetDlgCtrlID())->EnableWindow(FALSE);
	*/
}

void CSVCamSampleDlg::OnBnClickedQuit()
{

	if (currentSelected_Camera)
	{
		for (vector <SVCamFeaturInf*>::iterator it = currentSelected_Camera->sv_cam_feature->featureInfolist.begin(); it != currentSelected_Camera->sv_cam_feature->featureInfolist.end(); ++it)
		{
			currentSelected_Camera->sv_cam_feature->UnRegisterInvalidateCB((*it)->hFeature);
		}
	}

	//Close the streaming of the selected camera
	OnBnClickedStopStream();

	m_feature_tree.ShowWindow(SW_HIDE);
	m_feature_tree.DeleteAllItems();

	PostMessage(WM_QUIT, 0, NULL);
}

void CSVCamSampleDlg::OnEnEditFeatureString()
{
	DWORD_PTR data = m_feature_tree.GetItemData(selectedItem);
	SVCamFeaturInf *currentSelected_Featur = ((SVCamFeaturInf *)data);
	CString d;

	m_string_value.GetWindowTextW(d);

	CStringW cstrMyString(d);
	const size_t newsizew = (cstrMyString.GetLength() + 1) * 2;
	char *nstringw = new char[newsizew];
	size_t convertedCharsw = 0;
	wcstombs_s(&convertedCharsw, nstringw, newsizew, cstrMyString, _TRUNCATE);
	SVFeatureSetValueString(currentSelected_Featur->hRemoteDevice, currentSelected_Featur->hFeature, nstringw);

	delete nstringw;

	UpdateTreeItem(selectedItem);

}

void CSVCamSampleDlg::OnBnUpdateFeatureTree()
{

	if (currentSelected_Camera)
	{
		for (vector <SVCamFeaturInf*>::iterator it = currentSelected_Camera->sv_cam_feature->featureInfolist.begin(); it != currentSelected_Camera->sv_cam_feature->featureInfolist.end(); ++it)
		{
			currentSelected_Camera->sv_cam_feature->UnRegisterInvalidateCB((*it)->hFeature);
		}
	}

	UpdateFeatureTree();
}

BOOL CSVCamSampleDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_RETURN)// || pMsg->wParam == VK_ESCAPE)
		{

			DWORD_PTR data = m_feature_tree.GetItemData(selectedItem);
			SVCamFeaturInf *currentSelected_Featur = ((SVCamFeaturInf *)data);

			if (currentSelected_Featur->SVFeaturInf.type == SV_intfIString)
				OnEnEditFeatureString();

			if (currentSelected_Featur->SVFeaturInf.type == SV_intfIFloat || currentSelected_Featur->SVFeaturInf.type == SV_intfIInteger)
				OnEnFeatureEdit();

			return TRUE; // Do not process further
		}
	}

	return CWnd::PreTranslateMessage(pMsg);
}

void CSVCamSampleDlg::OnBnClickedStopStream()
{
	if (currentSelected_Camera && (!currentSelected_Camera->sv_cam_acq->acqTerminated))
	{
		BeginWaitCursor();


		if (!terminated)
		{
			terminated = true;
			WaitForSingleObject(m_acquisitionstopThread, INFINITE);
			PostThreadMessage(m_thread->m_nThreadID, 0, 0, 0);
		}
		ResetEvent(m_acquisitionstopThread);

		MSG msg;
		// Check if any messages are waiting in the queue
		while (PeekMessage(&msg, NULL, WM_DISPLAY_IMAGE, WM_DISPLAY_IMAGE, PM_REMOVE))
		{
			// Translate the message and dispatch it to WindowProc()
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}


		// clear display
		ShowImage(Display, 600, 600, display_Data);


		// Allocate a RGB buffer if needed
		// Delete additionial RGB buffer if needed
		if (ImageData_RGB != NULL)
		{
			GlobalFree(ImageData_RGB);
			ImageData_RGB = NULL;
		}

		// Delete additionial mono buffer if needed
		if (ImageData_MONO != NULL)
		{
			GlobalFree(ImageData_MONO);
			ImageData_MONO = NULL;
		}

		// Stop streamming of the currently selected camera
		currentSelected_Camera->sv_cam_acq->AcquisitionStop();

		EndWaitCursor();
		m_start_stream.ShowWindow(SW_SHOWNORMAL);
		m_stop_stream.ShowWindow(SW_HIDE);


		//invalidate the currently selected feature.
		m_edit_feature_value.ShowWindow(SW_HIDE);
		m_Feature_value.ShowWindow(SW_HIDE);
		m_command.ShowWindow(SW_HIDE);
		m_enumeration.ShowWindow(SW_HIDE);
		m_string_value.ShowWindow(SW_HIDE);

	}
}

void CSVCamSampleDlg::OnBnClickedStartStream()
{
	if (currentSelected_Camera && (currentSelected_Camera->sv_cam_acq->acqTerminated))
	{

		terminated = false;
		m_thread = AfxBeginThread(DisplayThreadfunction, this);
		BeginWaitCursor();

		currentSelected_Camera->sv_cam_acq->AcquisitionStart(4);
		m_start_stream.ShowWindow(SW_HIDE);
		m_stop_stream.ShowWindow(SW_SHOWNORMAL);

		EndWaitCursor();
		//invalidate the currently selected feature.
		m_edit_feature_value.ShowWindow(SW_HIDE);
		m_Feature_value.ShowWindow(SW_HIDE);
		m_command.ShowWindow(SW_HIDE);
		m_enumeration.ShowWindow(SW_HIDE);
		m_string_value.ShowWindow(SW_HIDE);

	}

}

void CSVCamSampleDlg::OnCbnSelchangeVisibility()
{
	int sl = m_feature_visibility.GetCurSel();
	currentFeatureVisibility = (SV_FEATURE_VISIBILITY)sl;

	HTREEITEM hRoot = this->m_feature_tree.GetRootItem();

	while (hRoot = m_feature_tree.GetNextVisibleItem(hRoot))
	{
		UpdateTreeItem(hRoot);
	}

	UpdateFeatureTree();
}

int CSVCamSampleDlg::SaveImage(size_t _Width, size_t _Height, unsigned char *ImageData)
{
	int Width = (int)_Width;
	int Height = (int)_Height;

	// Check image alignment
	if (Width % 4 == 0)
	{
		BITMAPINFO *bitmapinfo;

		// Generate and fill a bitmap info structure
		bitmapinfo = (BITMAPINFO *)new char[sizeof(BITMAPINFOHEADER)];

		bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapinfo->bmiHeader.biWidth = Width;
		bitmapinfo->bmiHeader.biHeight = -Height;
		bitmapinfo->bmiHeader.biBitCount = 24;
		bitmapinfo->bmiHeader.biPlanes = 1;
		bitmapinfo->bmiHeader.biClrUsed = 0;
		bitmapinfo->bmiHeader.biClrImportant = 0;
		bitmapinfo->bmiHeader.biCompression = BI_RGB;
		bitmapinfo->bmiHeader.biSizeImage = Width * Height * 3; //Width*Height*3 | 0
		bitmapinfo->bmiHeader.biXPelsPerMeter = 0;
		bitmapinfo->bmiHeader.biYPelsPerMeter = 0;

		// save to file_path
		__int64 now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		string fn = "C:\\images\\" + std::to_string(now) + ".bmp";
		FILE* pFile = fopen(fn.c_str(), "wb");
		//errno_t errorCode = fopen_s(&pFile, fn.c_str(), "w");

		if (pFile == NULL)
		{
			return 1;
		}

		BITMAPFILEHEADER bmfh;                         // Other BMP header
		int nBitsOffset = sizeof(BITMAPFILEHEADER) + bitmapinfo->bmiHeader.biSize;
		LONG lImageSize = bitmapinfo->bmiHeader.biSizeImage;
		LONG lFileSize = nBitsOffset + lImageSize;
		bmfh.bfType = 'B' + ('M' << 8);
		bmfh.bfOffBits = nBitsOffset;
		bmfh.bfSize = lFileSize;
		bmfh.bfReserved1 = bmfh.bfReserved2 = 0;

		// Write the bitmap file header               // Saving the first header to file
		UINT nWrittenFileHeaderSize = fwrite(&bmfh, 1, sizeof(BITMAPFILEHEADER), pFile);

		// And then the bitmap info header            // Saving the second header to file
		UINT nWrittenInfoHeaderSize = fwrite(&bitmapinfo->bmiHeader, 1, sizeof(BITMAPINFOHEADER), pFile);

		// Finally, write the image data itself
		//-- the data represents our drawing          // Saving the file content in lpBits to file
		UINT nWrittenDIBDataSize = fwrite(ImageData, 1, lImageSize, pFile);
		fclose(pFile); // closing the file.

		delete[] bitmapinfo;
		return 0;
	}

	return 1;
}