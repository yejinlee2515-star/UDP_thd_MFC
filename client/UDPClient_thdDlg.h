
// UDPClient_thdDlg.h : ��� ����
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

// CUDPClient_thdDlg ��ȭ ����
class CUDPClient_thdDlg : public CDialogEx
{
// �����Դϴ�.
public:
	CUDPClient_thdDlg(CWnd* pParent = NULL);	// ǥ�� �������Դϴ�.

// ��ȭ ���� �������Դϴ�.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_UDPCLIENT_THD_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �����Դϴ�.


// �����Դϴ�.
protected:
	HICON m_hIcon;

	// ������ �޽��� �� �Լ�
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
