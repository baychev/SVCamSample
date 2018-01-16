
#include "stdafx.h"
#include "afxwin.h"
#include "afxcmn.h"
#include "SVCamSample.h"

#include "SVCamSystem.h"
#include "SVCamFeature.h"
#include "SVCamAcquisation.h"

#define WM_DISPLAY_IMAGE	(WM_APP + 400)


// CSVCamSampleDlg dialog
class CSVCamSampleDlg : public CDialogEx
{

public:
	CSVCamSampleDlg(CWnd* pParent = NULL);
	~CSVCamSampleDlg();

	Camera * GetCamera(SVCamSystem* svcam, char* id);
	bool OpenSelectedCam(SVCamSystem* svcam, size_t CamIndex, size_t buffercount);


	void ShowImage(HDC DisplayDC, size_t Width, size_t Height, unsigned char *ImageData);
	void ShowImageRGB(HDC DisplayDC, size_t Width, size_t Height, unsigned char *ImageData);

	// Feature tree specific functions
	void AddFeatureToTree(SVCamFeaturInf* curentfeature, HTREEITEM *root);


	void UpdateFeatureTree();
	HTREEITEM FindItemByName(const CString& name, CTreeCtrl& tree, HTREEITEM hRoot);
	HTREEITEM FindItemByLevel(int Level, CTreeCtrl& tree, HTREEITEM hRoot);
	void OnFeatureInvalidated(const char *featureName);
	void UpdateTreeItem(HTREEITEM hRoot);
	HTREEITEM  selectedItem;
	SV_FEATURE_VISIBILITY  currentFeatureVisibility;

	// Dialog Data
	enum { IDD = IDD_SVCAMSAMPLE_DIALOG };
	SVCamSystem  *usbCam;
	SVCamSystem  *gigeCam;
	SVCamSystem  *clCam;
	Camera *currentSelected_Camera;

	// Acquisition thread parameter
	bool	terminated;
	CWinThread *m_thread;
	HANDLE m_acquisitionstopThread;

private:
	// A device context for displaying images
	HDC Display;
	int DisplayWidth;
	int DisplayHeight;
	//SV_BUFFER_INFO *ImageInfo;
	unsigned char * ImageData_RGB;
	unsigned char * ImageData_MONO;
	unsigned char 	*display_Data;


protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Function for displaying recieved images. 
	LRESULT WMDisplayImage(WPARAM WParam, LPARAM LParam);
	HICON m_hIcon;
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg HCURSOR OnQueryDragIcon();
	BOOL PreTranslateMessage(MSG* pMsg);
	DECLARE_MESSAGE_MAP()

public:

	//device selection
	afx_msg void OnCbnSelchangeUsbCam();
	afx_msg void OnCbnSelchangeGigeCam();
	afx_msg void OnCbnSelchangeCameraLink();
	void openSelectedCam(SVCamSystem  *type_Cam);

	// camera feature specific functions 
	afx_msg void OnBnClickedFeatureCommand();
	afx_msg void OnDrawFeatureSlider(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSelectEnumFeature();
	afx_msg void OnDblclkFeatureTree(NMHDR *pNMHDR, LRESULT *pResult);

	CComboBox m_usb_cameras;
	CComboBox m_gige_cameras;
	CComboBox m_cl_cameras;

	// Stream Acquisition specific functions 
	afx_msg void OnBnClickedStopStream();
	afx_msg void OnBnClickedStartStream();
	CButton m_stop_stream;
	CButton m_start_stream;
	CEdit m_frame_id;
	CStatic m_display;

	CTreeCtrl m_feature_tree;
	CButton m_command;
	CComboBox m_enumeration;
	CSliderCtrl m_Feature_value;
	CEdit m_edit_feature_value;
	CEdit m_tool_tip;
	CEdit m_current_feature;

	afx_msg void OnBnClickedQuit();

	CEdit m_string_value;
	afx_msg void OnEnEditFeatureString();

	CButton m_update_tree;
	afx_msg void OnBnUpdateFeatureTree();

	afx_msg void OnReleasedcaptureFeatureSlider(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnFeatureEdit();

	CComboBox m_feature_visibility;
	afx_msg void OnCbnSelchangeVisibility();
	int SaveImage(size_t _Width, size_t _Height, unsigned char * ImageData);
};
