
// UDPClient_thdDlg.cpp : 구현 파일
//

#include "stdafx.h"
#include "UDPClient_thd.h"
#include "UDPClient_thdDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SEGMENTATION 8

CCriticalSection tx_cs;
CCriticalSection rx_cs;

int _seq_num = 0;

// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
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


// CUDPClient_thdDlg 대화 상자



CUDPClient_thdDlg::CUDPClient_thdDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_UDPCLIENT_THD_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUDPClient_thdDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_tx_edit_short);
	DDX_Control(pDX, IDC_EDIT2, m_tx_edit);
	DDX_Control(pDX, IDC_EDIT3, m_rx_edit);
	DDX_Control(pDX, IDC_IPADDRESS1, m_ipaddr);
}

BEGIN_MESSAGE_MAP(CUDPClient_thdDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_SEND, &CUDPClient_thdDlg::OnBnClickedSend)
	ON_BN_CLICKED(IDC_CLOSE, &CUDPClient_thdDlg::OnBnClickedClose)
END_MESSAGE_MAP()


// CUDPClient_thdDlg 메시지 처리기

bool Checksum(Frame frame)
{
	unsigned short sum = 0;
	unsigned short result;
	unsigned short checksum = 0;

	for (int i = 0; i < sizeof(frame.p_buffer) / 2; i++)
	{
		sum += frame.p_buffer[i];
	}
	sum += frame.checksum;

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
	CList<Frame> *plist = pArg->pList;	// 전달받은 pLIst 저장
	CUDPClient_thdDlg *pDlg = (CUDPClient_thdDlg *)pArg->pDlg;	// 대화상자를 가리키는 변수
	Frame frame;
	CString message;
	CString seq_message;
	CString Mid[16];

	while (pArg->Thread_run)	// Thread가 run인 동안 실행하도록 함
	{
		POSITION pos = plist->GetHeadPosition();
		POSITION current_pos;
		while (pos != NULL)
		{
			current_pos = pos;	// pList의 가장 앞
			rx_cs.Lock();	// cs
			frame = plist->GetNext(pos);	// 현재 위치가 바뀔 수 있으므로 cs에 넣어 줌
			rx_cs.Unlock();

			if (Checksum(frame) != 0x0000)      //Sender의 checksum과 Receiver의 checksum비교
			{
				AfxMessageBox(_T("checksum error !"));         //checksum이 0이아니면 오류발생
				rx_cs.Lock();
				plist->RemoveAt(current_pos);               //position 메세지를 삭제
				rx_cs.Unlock(); 
				continue;
			}
			else
			{
				AfxMessageBox(_T("checksum success !"));      //checksum이 0이면 오류없음
			}
			
			_seq_num += frame.seq_num;
			frame.ack_num = _seq_num + 1;
			frame.is_ack = 1;
			pDlg->m_pDataSocket->SendToEx(&frame, sizeof(Frame), 8000, _T("127.0.0.1"), 0);


			// reassembly
			
			CString temp = _T("");
			Frame _frame = frame;

			if (frame.seq_num == frame.length / SEGMENTATION)
			{
				Mid[frame.seq_num] = (LPCTSTR)frame.p_buffer;
				for (int i = 0; i <= frame.length / SEGMENTATION; i++)
				{
					temp += Mid[i];
					Mid[i] = _T("");
				}
			}
			else
			{
				Mid[frame.seq_num] = (LPCTSTR)frame.p_buffer;
				rx_cs.Lock();
				plist->RemoveAt(current_pos);               //position 메세지를 삭제
				rx_cs.Unlock();
				continue;
			}

			/*
			temp += frame.p_buffer;

			if (frame.seq_num == frame.length / SEGMENTATION)
			{
				_tcscpy_s(_frame.p_buffer, (LPCTSTR)temp);
				temp = _T("");
			}
			else
			{
				rx_cs.Lock();
				plist->RemoveAt(current_pos);
				rx_cs.Unlock();
				continue;
			}*/

			seq_message.Format(_T("%d"), _seq_num);
			AfxMessageBox(seq_message + _T("번 프레임 도착"));
			
			int len = pDlg->m_rx_edit.GetWindowTextLengthW();
			pDlg->m_rx_edit.SetSel(len, len);
			pDlg->m_rx_edit.ReplaceSel(_T(" ") + temp);
			temp = _T("");
			rx_cs.Lock();
			plist->RemoveAt(current_pos);               //position 메세지를 삭제
			rx_cs.Unlock();

			/*
			pDlg->m_rx_edit.GetWindowText(message);
			message += _frame.p_buffer;
			pDlg->m_rx_edit.SetWindowText(message);	// 화면에 출력
			pDlg->m_rx_edit.LineScroll(pDlg->m_rx_edit.GetLineCount());	// 할당된 칸을 모두 사용하면 자동으로 스크롤 되도록 함
*/
		}
		Sleep(10);
	}
	return 0;
}

UINT TXThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg*)arg;
	CList<Frame> *plist = pArg->pList;	// 입력받은 pLIst 저장
	CUDPClient_thdDlg *pDlg = (CUDPClient_thdDlg *)pArg->pDlg;	// 대화상자를 가리키는 변수
	
	while (pArg->Thread_run)	// Thread가 run인 동안 실행하도록 함
	{
		POSITION pos = plist->GetHeadPosition();
		POSITION current_pos;
		while (pos != NULL)
		{
			current_pos = pos;	// pList의 가장 앞
			tx_cs.Lock();	// cs
			Frame frame = plist->GetNext(pos);	// 현재 위치가 바뀔 수 있으므로 cs에 넣어 줌
			frame.checksum = Checksum(frame);
			tx_cs.Unlock();	// cs 해제

			pDlg->m_pDataSocket->SendToEx(&frame, sizeof(Frame), 8000, _T("127.0.0.1"), 0);
			/*
			while (1)
			{
			pDlg->m_pDataSocket->SendToEx(&str, sizeof(Packet), 8000, _T("127.0.0.1"), 0);
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

			}
			*/
			pDlg->m_tx_edit.LineScroll(pDlg->m_tx_edit.GetLineCount());

			plist->RemoveAt(current_pos);	// 처리를 완료하였으므로 현재 메세지 삭제 
		}
		Sleep(10);
	}
	return 0;
}

BOOL CUDPClient_thdDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
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

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	CList<Frame>* newlist = new CList<Frame>;
	arg1.pList = newlist;
	arg1.Thread_run = 1;
	arg1.pDlg = this;

	CList<Frame>* newlist2 = new CList<Frame>;
	arg2.pList = newlist2;
	arg2.Thread_run = 1;
	arg2.pDlg = this;

	m_ipaddr.SetWindowTextW(_T("127.0.0.1"));

	WSADATA wsa;
	int error_code;
	if ((error_code = WSAStartup(MAKEWORD(2, 2), &wsa)) != 0)
	{
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	}

	m_pDataSocket = new CDataSocket(this);
	if (m_pDataSocket->Create(7000, SOCK_DGRAM))
	{
		AfxMessageBox(_T("Client를 시작합니다."), MB_ICONINFORMATION);
		pThread1 = AfxBeginThread(TXThread, (LPVOID)&arg1);	// TXThread 시작
		pThread2 = AfxBeginThread(RXThread, (LPVOID)&arg2);	// RXThread 시작
		return TRUE;
	}
	else
	{
		int err = m_pDataSocket->GetLastError();
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	}

	AfxMessageBox(_T("이미 실행중인 서버가 있습니다.") _T("\n프로그램을 종료합니다."), MB_ICONERROR);


	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CUDPClient_thdDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 응용 프로그램의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CUDPClient_thdDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CUDPClient_thdDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CUDPClient_thdDlg::OnBnClickedSend()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	CString tx_message;
	Frame frame;
	int i = 0;

	m_tx_edit_short.GetWindowText(tx_message);

	// segmentation

	frame.length = tx_message.GetLength();

	if ((frame.length / SEGMENTATION) == 0)
	{

		tx_message += _T("\r\n");
		_tcscpy_s(frame.p_buffer, (LPCTSTR)tx_message);

		frame.seq_num = 0;

		tx_cs.Lock();//임계구역 진입
		arg1.pList->AddTail(frame);// 보내는 리스트에 프레임 실어보냄
		tx_cs.Unlock();//임계구역 해제
	}
	else
	{
		for (i = 0; i < frame.length / SEGMENTATION; i++)
		{
			_tcscpy_s(frame.p_buffer, (LPCTSTR)tx_message.Mid(i*SEGMENTATION, SEGMENTATION));
			frame.seq_num = i;
			tx_cs.Lock();
			arg1.pList->AddTail(frame);
			tx_cs.Unlock();
		}

		tx_message += _T("\r\n");

		CString seg;
		seg = (LPCTSTR)tx_message.Mid(i*SEGMENTATION, frame.length - i*SEGMENTATION + 3);
		_tcscpy_s(frame.p_buffer, seg);
		frame.seq_num = i;

		tx_cs.Lock();
		arg1.pList->AddTail(frame);
		tx_cs.Unlock();
	}

	m_tx_edit_short.SetWindowTextW(_T(""));
	m_tx_edit_short.SetFocus();

	int len = m_tx_edit.GetWindowTextLengthW();
	m_tx_edit.SetSel(len, len);
	m_tx_edit.ReplaceSel(_T("") + tx_message);
}


void CUDPClient_thdDlg::OnBnClickedClose()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	if (m_pDataSocket == NULL)
	{
		MessageBox(_T("서버에 접속 안 함!"), _T("알림"), MB_ICONERROR);
	}
	else // 접속되어 있는 경우
	{
		arg1.Thread_run = 0;
		arg2.Thread_run = 0;
		m_pDataSocket->Close();
		delete m_pDataSocket;
		m_pDataSocket = NULL; // data 소켓 삭제
	}
}


void CUDPClient_thdDlg::ProcessReceive(CDataSocket* pSocket, int nErrorCode)
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

	if (frame.is_ack == 0)
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
	else
	{
		CString ack_message;
		ack_message.Format(_T("%d"), frame.ack_num);
		AfxMessageBox(ack_message + _T("번 ack 도착"));
	}
}


void CUDPClient_thdDlg::ProcessClose(CDataSocket* pSocket, int nErrorCode)
{
	arg1.Thread_run = 0;
	arg2.Thread_run = 0;
	pSocket->Close();
	delete m_pDataSocket;
	m_pDataSocket = NULL;

	int len = m_rx_edit.GetWindowTextLengthW();
	CString message = _T("### 접속 종료 ###\r\n\r\n");
	m_rx_edit.SetSel(len, len);
	m_rx_edit.ReplaceSel(message);
}


// seqence: 보낸 데이터 번호, 보낼 때 마다 증가!
// ack: 받을 수 있는 데이터의 시퀀스