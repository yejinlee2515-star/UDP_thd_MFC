
// UDPServer_thdDlg.cpp : ���� ����
//

#include "stdafx.h"
#include "UDPServer_thd.h"
#include "UDPServer_thdDlg.h"
#include "afxdialogex.h"
#include "DataSocket.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SEGMENTATION 8

bool Timer_Run = FALSE;
bool Timeout = FALSE;

CCriticalSection tx_cs;
CCriticalSection rx_cs;

int _seq_num = 0;	// ���� sequence number, ���������� ����

// ���� ���α׷� ������ ���Ǵ� CAboutDlg ��ȭ �����Դϴ�.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// ��ȭ ���� �������Դϴ�.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �����Դϴ�.

// �����Դϴ�.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CUDPServer_thdDlg ��ȭ ����



CUDPServer_thdDlg::CUDPServer_thdDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_UDPSERVER_THD_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUDPServer_thdDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_tx_edit_short);
	DDX_Control(pDX, IDC_EDIT2, m_tx_edit);
	DDX_Control(pDX, IDC_EDIT3, m_rx_edit);
}

BEGIN_MESSAGE_MAP(CUDPServer_thdDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_SEND, &CUDPServer_thdDlg::OnBnClickedSend)
	ON_BN_CLICKED(IDC_Close, &CUDPServer_thdDlg::OnBnClickedClose)
END_MESSAGE_MAP()


// CUDPServer_thdDlg �޽��� ó����
UINT TimerThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg*)arg;

	while (pArg->Thread_run)
	{
		if (Timer_Run == TRUE) {
			int SendOut = GetTickCount();

			while (1) {
				if ((Timer_Run == FALSE)) {
					break;
				}
				if ((GetTickCount() - SendOut) / CLOCKS_PER_SEC > 10)
				{
					Timeout = TRUE;
					break;
				}
			}
		}
		Sleep(100);
	}
	return 0;
}
int Checksum(Frame frame)
{
	unsigned short sum = 0;
	unsigned short result;
	unsigned short checksum = 0;

	for (int i = 0; i < sizeof(frame.p_buffer) / 2; i++)
	{
		sum += frame.p_buffer[i];
	}
	sum += frame.checksum;	// �ʱ� checksum�� ���� 0

	result = sum & 0xFFFF;
	result = ~result + 1;

	result += sum;
	result &= 0xFFFF;
	checksum = result;

	return (unsigned short)checksum;
}
UINT RXThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg*)arg;
	CList<Frame> *plist = pArg->pList;	// ���޹��� pLIst ����
	CUDPServer_thdDlg *pDlg = (CUDPServer_thdDlg *)pArg->pDlg;	// ��ȭ���ڸ� ����Ű�� ����
	Frame frame;
	CString message;
	CString seq_message;
	CString Mid[16];

	while (pArg->Thread_run)	// Thread�� run�� ���� �����ϵ��� ��
	{
		POSITION pos = plist->GetHeadPosition();
		POSITION current_pos;
		while (pos != NULL)
		{
			current_pos = pos;	// pList�� ���� ��
			rx_cs.Lock();	// cs
			frame = plist->GetNext(pos);	// ���� ��ġ�� �ٲ� �� �����Ƿ� cs�� �־� ��
			rx_cs.Unlock();

			// checksum, frame.checksum�� sender�� checksum�� ���� �����Ƿ� Checksum�� ����� 0x0000�̾�� �������� ���̴�.
			if (Checksum(frame) != 0x0000)
			{
				AfxMessageBox(_T("checksum error !"));
				rx_cs.Lock();
				plist->RemoveAt(current_pos);	//�߸� ������ ���̹Ƿ� �޼��� ����
				rx_cs.Unlock();
				continue;
			}
			else
			{
				AfxMessageBox(_T("checksum success !"));      //checksum�� 0�̸� ��������
			}

			_seq_num += frame.seq_num;	// sequence number, ���� �� ���� 1�� �����ϴ� ��, frame.seq_num�� �ش� �޼��� ���ο����� ������
			frame.ack_num = _seq_num + 1;	// acknowledge number �޼����� �޾Ұ� ack_num��° �޼����� ���� �غ� �Ǿ����� ǥ��
			frame.is_ack = 1;	// �ش� �������� acknowledgeȮ�� �޼����� �� 1, �ƴϸ� 0
			pDlg->m_pDataSocket->SendToEx(&frame, sizeof(Frame), pArg->ClientPort, pArg->ClientAddr, 0);	// �۽źο� ack �޼��� ����

			// reassembly
			CString temp = _T("");
			Frame _frame = frame;

			if (frame.seq_num == frame.length / SEGMENTATION)	// sequence number�� segmentation�� �������� ��ȣ�� ��ġ�� ���
			{
				Mid[frame.seq_num] = (LPCTSTR)frame.p_buffer;	// Mid�� message�� ����
				for (int i = 0; i <= frame.length / SEGMENTATION; i++)
				{
					temp += Mid[i];
					Mid[i] = _T("");
				}
			}
			else // �ٸ� ���
			{
				Mid[frame.seq_num] = (LPCTSTR)frame.p_buffer;
				rx_cs.Lock();
				plist->RemoveAt(current_pos);	// �޼��� ����
				rx_cs.Unlock();
				continue;	// �Ʒ� ��ɾ� �ǳʶٰ� ���� ������ �̵�
			}

			seq_message.Format(_T("%d"), _seq_num);
			AfxMessageBox(seq_message + _T("�� ������ ����"));

			// ȭ�鿡 ���
			int len = pDlg->m_rx_edit.GetWindowTextLengthW();
			pDlg->m_rx_edit.SetSel(len, len);
			pDlg->m_rx_edit.ReplaceSel(_T(" ") + temp);
			temp = _T("");
			rx_cs.Lock();
			plist->RemoveAt(current_pos);	// ���� ��ġ �޼����� ��� ó���Ͽ����Ƿ� ����
			rx_cs.Unlock();
		}
		Sleep(10);
	}
	return 0;

}
UINT TXThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg*)arg;
	CList<Frame> *plist = pArg->pList;	// �Է¹��� pLIst ����
	CUDPServer_thdDlg *pDlg = (CUDPServer_thdDlg *)pArg->pDlg;	// ��ȭ���ڸ� ����Ű�� ����
	Frame frame;

	while (pArg->Thread_run)	// Thread�� run�� ���� �����ϵ��� ��
	{
		POSITION pos = plist->GetHeadPosition();
		POSITION current_pos;
		while (pos != NULL)
		{
			current_pos = pos;	// pList�� ���� ��
			
			tx_cs.Lock();	// cs
			frame = plist->GetNext(pos);	// ���� ��ġ�� �ٲ� �� �����Ƿ� cs�� �־� ��
			frame.checksum = Checksum(frame);
			tx_cs.Unlock();	// cs ����

			// ���������� �۽��� Ŭ���̾�Ʈ�� ��Ʈ��ȣ�� �ּҷ� �޼��� ����
			if (pDlg->m_pDataSocket->SendToEx(&frame, sizeof(Frame), pArg->ClientPort, pArg->ClientAddr) < 0)
			{
				AfxMessageBox(_T("���� ����"));
			}
			
			/*
			while (1)
			{
				pDlg->m_pDataSocket->SendToEx(&frame, sizeof(Frame), pArg->ClientPort, pArg->ClientAddr, 0);
				Timeout = FALSE;
				if (Timeout == TRUE)
				{
					int len = pDlg->m_tx_edit.GetWindowTextLengthW();
					CString message = _T("TimeOut!\r\n");
					pDlg->m_tx_edit.SetSel(len, len);
					pDlg->m_tx_edit.ReplaceSel(message);

					Sleep(10);
					break;
				}
			}*/
			

			pDlg->m_tx_edit.LineScroll(pDlg->m_tx_edit.GetLineCount());

			plist->RemoveAt(current_pos);	// ���� �޼��� ���� 


		}
		Sleep(10);
	}
	return 0;
}
BOOL CUDPServer_thdDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// �ý��� �޴��� "����..." �޴� �׸��� �߰��մϴ�.

	// IDM_ABOUTBOX�� �ý��� ��� ������ �־�� �մϴ�.
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

	// �� ��ȭ ������ �������� �����մϴ�.  ���� ���α׷��� �� â�� ��ȭ ���ڰ� �ƴ� ��쿡��
	//  �����ӿ�ũ�� �� �۾��� �ڵ����� �����մϴ�.
	SetIcon(m_hIcon, TRUE);			// ū �������� �����մϴ�.
	SetIcon(m_hIcon, FALSE);		// ���� �������� �����մϴ�.

	// TODO: ���⿡ �߰� �ʱ�ȭ �۾��� �߰��մϴ�.
	CList<Frame>* newlist = new CList<Frame>;
	arg1.pList = newlist;
	arg1.Thread_run = 1;
	arg1.pDlg = this;

	CList<Frame>* newlist2 = new CList<Frame>;
	arg2.pList = newlist2;
	arg2.Thread_run = 1;
	arg2.pDlg = this;

	WSADATA wsa;
	int error_code;
	if ((error_code = WSAStartup(MAKEWORD(2, 2), &wsa)) != 0)
	{
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	}

	m_pDataSocket = new CDataSocket(this);
	if (m_pDataSocket->Create(8000, SOCK_DGRAM))
	{
		AfxMessageBox(_T("������ �����մϴ�."), MB_ICONINFORMATION);
		pThread1 = AfxBeginThread(TXThread, (LPVOID)&arg1);	// TXThread ����
		pThread2 = AfxBeginThread(RXThread, (LPVOID)&arg2);	// RXThread ����
		return TRUE;
	}
	else
	{
		int err = m_pDataSocket->GetLastError();
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	}

	AfxMessageBox(_T("�̹� �������� ������ �ֽ��ϴ�.") _T("\n���α׷��� �����մϴ�."), MB_ICONERROR);


	return TRUE;  // ��Ŀ���� ��Ʈ�ѿ� �������� ������ TRUE�� ��ȯ�մϴ�.
}

void CUDPServer_thdDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// ��ȭ ���ڿ� �ּ�ȭ ���߸� �߰��� ��� �������� �׸�����
//  �Ʒ� �ڵ尡 �ʿ��մϴ�.  ����/�� ���� ����ϴ� MFC ���� ���α׷��� ��쿡��
//  �����ӿ�ũ���� �� �۾��� �ڵ����� �����մϴ�.

void CUDPServer_thdDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // �׸��⸦ ���� ����̽� ���ؽ�Ʈ�Դϴ�.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Ŭ���̾�Ʈ �簢������ �������� ����� ����ϴ�.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �������� �׸��ϴ�.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// ����ڰ� �ּ�ȭ�� â�� ���� ���ȿ� Ŀ���� ǥ�õǵ��� �ý��ۿ���
//  �� �Լ��� ȣ���մϴ�.
HCURSOR CUDPServer_thdDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CUDPServer_thdDlg::ProcessReceive(CDataSocket* pSocket, int nErrorCode)
{
	Frame frame;
	int nbytes;
	CString PeerAddr;
	UINT PeerPort;

	
	if (pSocket->ReceiveFromEx(&frame, sizeof(Frame), PeerAddr, PeerPort) < 0)
	{
		AfxMessageBox(_T("Receive Error"), MB_ICONERROR);
	}
	/*
	rx_cs.Lock();
	arg2.pList->AddTail(frame);
	rx_cs.Unlock();*/

	if (frame.is_ack == 0)	// ��Ʈ��ȣ�� �ּ� �޾ƿ��� ����Ʈ�� �޼��� �߰�
	{
		rx_cs.Lock();
		arg2.ClientPort = PeerPort;
		arg2.ClientAddr = (LPCTSTR)PeerAddr;
		arg2.pList->AddTail(frame);
		rx_cs.Unlock();
		rx_cs.Lock();
		arg1.ClientAddr = (LPCTSTR)PeerAddr;
		arg1.ClientPort = PeerPort;
		rx_cs.Unlock();
	}
	else // ack�޼����� ��� �޼����ڽ� ���
	{
		CString ack_message;
		ack_message.Format(_T("%d"), frame.ack_num);
		AfxMessageBox(ack_message + _T("�� ack ����"));
	}
	
}


void CUDPServer_thdDlg::ProcessClose(CDataSocket* pSocket, int nErrorCode)
{
	arg1.Thread_run = 0;
	arg2.Thread_run = 0;
	pSocket->Close();
	delete m_pDataSocket;
	m_pDataSocket = NULL;

	int len = m_rx_edit.GetWindowTextLengthW();
	CString message = _T("### ���� ���� ###\r\n\r\n");
	m_rx_edit.SetSel(len, len);
	m_rx_edit.ReplaceSel(message);
}


void CUDPServer_thdDlg::OnBnClickedSend()
{
	// TODO: ���⿡ ��Ʈ�� �˸� ó���� �ڵ带 �߰��մϴ�.
	CString tx_message;
	Frame frame;
	int i = 0;

	if (arg2.ClientPort == -1) {
		MessageBox(_T("���ŵ� client ����"), _T("���� !"), MB_ICONERROR);
		m_tx_edit_short.SetWindowText(_T(""));
		return;
	}

	m_tx_edit_short.GetWindowText(tx_message);	// ���� �޾ƿ�

	// segmentation

	frame.length = tx_message.GetLength();

	if ((frame.length / SEGMENTATION) == 0)	// �޼��� ���̰� segmentation�� ���̺��� ª�� ���
	{

		tx_message += _T("\r\n");
		_tcscpy_s(frame.p_buffer, (LPCTSTR)tx_message);	// �����ӿ� �޼��� ����

		frame.seq_num = 0;	// segmentation�� �� ����� �ϳ��� �������̹Ƿ� �ش� �޼��� ���ο����� �������� 0�̴�.

		tx_cs.Lock();
		arg1.pList->AddTail(frame);		// ����Ʈ�� �������� �߰�
		tx_cs.Unlock();
	}
	else	// �޼��� ���̰� segmentation�� ���̺��� �� ���
	{
		for (i = 0; i < frame.length / SEGMENTATION; i++)	// segmentation ��� ������� ������ ��
		{
			// �����ӿ� �޼����� i*SEGMENTATION��°���� SEGMENTATION���� ���ڸ� ����
			_tcscpy_s(frame.p_buffer, (LPCTSTR)tx_message.Mid(i*SEGMENTATION, SEGMENTATION));
			frame.seq_num=i;	// �޼��� ���ο����� ������
			tx_cs.Lock();
			arg1.pList->AddTail(frame);	// ����Ʈ�� segmentation�� ������ �߰�
			tx_cs.Unlock();
		}

		tx_message += _T("\r\n");

		// message�� ���� ���� ó��
		CString seg;
		seg = (LPCTSTR)tx_message.Mid(i*SEGMENTATION, frame.length - i*SEGMENTATION + 3);
		_tcscpy_s(frame.p_buffer, seg);	// �����ӿ� �޼��� �־���
		frame.seq_num=i;	// �޼��� ���ο����� ������

		tx_cs.Lock();
		arg1.pList->AddTail(frame);	// ����Ʈ�� segmentation�� ������ �߰�
		tx_cs.Unlock();
	}

	// �޼��� ȭ�鿡 ���
	m_tx_edit_short.SetWindowTextW(_T(""));
	m_tx_edit_short.SetFocus();

	int len = m_tx_edit.GetWindowTextLengthW();
	m_tx_edit.SetSel(len, len);
	m_tx_edit.ReplaceSel(_T("") + tx_message);
}


void CUDPServer_thdDlg::OnBnClickedClose()
{
	// TODO: ���⿡ ��Ʈ�� �˸� ó���� �ڵ带 �߰��մϴ�.
	if (m_pDataSocket == NULL)
	{
		MessageBox(_T("������ ���� �� ��!"), _T("�˸�"), MB_ICONERROR);
	}
	else // ���ӵǾ� �ִ� ���
	{
		arg1.Thread_run = 0;
		arg2.Thread_run = 0;
		m_pDataSocket->Close();
		delete m_pDataSocket;
		m_pDataSocket = NULL; // data ���� ����
	}
}
