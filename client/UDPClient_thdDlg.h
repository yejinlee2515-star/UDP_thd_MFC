
// UDPClient_thdDlg.h : 헤더 파일
//

#pragma once
#include "afxwin.h"
#include "DataSocket.h"
#include "afxcmn.h"

struct Frame
{
	int seq_num = 0;
	int ack_num = 0;
	unsigned short checksum = 0;
	TCHAR p_buffer[256];
	int length = 0;
	int is_ack = 0;
	Frame() {}
};

struct ThreadArg
{
	CList<Frame>* pList;
	CDialogEx* pDlg;
	int Thread_run;
	UINT ClientPort = -1;
	CString ClientAddr;
};

// CUDPClient_thdDlg 대화 상자
class CUDPClient_thdDlg : public CDialogEx
{
// 생성입니다.
public:
	CUDPClient_thdDlg(CWnd* pParent = NULL);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_UDPCLIENT_THD_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CWinThread *pThread1, *pThread2;
	ThreadArg arg1, arg2;
	CDataSocket *m_pDataSocket;
	CEdit m_tx_edit_short;
	CEdit m_tx_edit;
	CEdit m_rx_edit;
	afx_msg void OnBnClickedSend();
	afx_msg void OnBnClickedClose();
	void ProcessReceive(CDataSocket* pSocket, int nErrorCode);
	void ProcessClose(CDataSocket* pSocket, int nErrorCode);
	CIPAddressCtrl m_ipaddr;
};
