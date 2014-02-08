#include "stdafx.h"
#include "utils.h"
#include "dump_filter.h"
#include "intel_usage.h"


// paths for binaries for debugging
#ifdef _DEBUG
#define PATH_MERGE "..\\merge\\Release\\"
#define PATH_DECODER "..\\bin\\"
#define PATH_SPLITTER L"..\\bin\\"
#else
#define PATH_MERGE
#define PATH_DECODER
#define PATH_SPLITTER
#endif


extern string program_path;

#define FILTER_NAME "ssifSource"
#define FRAME_START -1
#define FRAME_BLACK -2

std::string IntToStr(int a) {
	char buffer[32];
	_itoa_s(a, buffer, 32, 10);
	return buffer;
}

PClip ClipStack(IScriptEnvironment* env, PClip a, PClip b, bool horizontal) {
	const char* arg_names[2] = {NULL, NULL};
	AVSValue args[2] = {a, b};
	return (env->Invoke(horizontal ? "StackHorizontal" : "StackVertical", AVSValue(args,2), arg_names)).AsClip();
}

PVideoFrame FrameStack(IScriptEnvironment* env, VideoInfo& vi, PVideoFrame a, PVideoFrame b, bool horizontal) {
	return ClipStack(env, new FrameHolder(vi, a), new FrameHolder(vi, b), horizontal)->GetFrame(0, env);
}




string SSIFSource::MakePipeName(int id, const string& name) {
	return (string)"\\\\.\\pipe\\bluray" + IntToStr(id) + "\\" + name;
}

HRESULT CreateDumpFilter(const IID& riid, LPVOID *pFilter) {
//  for debugging with installed filter
//	return CoCreateInstance(CLSID_DumpFilter, NULL, CLSCTX_INPROC_SERVER, riid, pFilter);

	HRESULT hr = S_OK;
	CUnknown *filter = CDump::CreateInstance(NULL, &hr);
	if (SUCCEEDED(hr))
		return filter->NonDelegatingQueryInterface(riid, pFilter);
	return hr;
}

HRESULT SSIFSource::CreateGraph(const WCHAR* fnSource, const WCHAR* fnBase, const WCHAR* fnDept,
					CComPtr<IGraphBuilder>& poGraph, CComPtr<IBaseFilter>& poSplitter)
{
	HRESULT hr = S_OK;
	IGraphBuilder *pGraph = NULL;
	CComQIPtr<IBaseFilter> pSplitter;
	CComQIPtr<IBaseFilter> pDumper1, pDumper2;
	LPOLESTR lib_Splitter = T2OLE(PATH_SPLITTER L"MpegSplitter_mod.ax");
	const CLSID *clsid_Splitter = &CLSID_MpegSplitter;

	WCHAR fullname_splitter[MAX_PATH+64];
	size_t len;
	if (GetModuleFileNameW(hInstance, fullname_splitter, MAX_PATH) == 0)
		return E_UNEXPECTED;
	StringCchLengthW(fullname_splitter, MAX_PATH, &len);
	while (len > 0 && fullname_splitter[len-1] != '\\') --len;
	fullname_splitter[len] = '\0';
	StringCchCatW(fullname_splitter, MAX_PATH+64, lib_Splitter);

	// Create the Filter Graph Manager.
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&pGraph);
	if (FAILED(hr)) goto lerror;
	hr = DSHelpCreateInstance(fullname_splitter, *clsid_Splitter, NULL, IID_IBaseFilter, (void**)&pSplitter);
	if (FAILED(hr)) goto lerror;
	hr = CreateDumpFilter(IID_IBaseFilter, (void**)&pDumper1);
	if (FAILED(hr)) goto lerror;
	if (fnDept) {
		hr = CreateDumpFilter(IID_IBaseFilter, (void**)&pDumper2);
		if (FAILED(hr)) goto lerror;
	}

	hr = pGraph->AddFilter(pSplitter, L"VSplitter");
	if (FAILED(hr)) goto lerror;
	hr = CComQIPtr<IFileSourceFilter>(pSplitter)->Load(fnSource, NULL);
	if (FAILED(hr)) goto lerror;

	hr = pGraph->AddFilter(pDumper1, L"VDumper1");
	if (FAILED(hr)) goto lerror;
	hr = CComQIPtr<IFileSinkFilter>(pDumper1)->SetFileName(fnBase, NULL);
	if (FAILED(hr)) goto lerror;

	if (pDumper2) {
		hr = pGraph->AddFilter(pDumper2, L"VDumper2");
		if (FAILED(hr)) goto lerror;
		hr = CComQIPtr<IFileSinkFilter>(pDumper2)->SetFileName(fnDept, NULL);
		if (FAILED(hr)) goto lerror;
	}

	hr = pGraph->ConnectDirect(GetOutPin(pSplitter, 0, true), GetInPin(pDumper1, 0), NULL);
	if (FAILED(hr)) goto lerror;
	if (pDumper2) {
		hr = pGraph->ConnectDirect(GetOutPin(pSplitter, 1, true), GetInPin(pDumper2, 0), NULL);
		if (FAILED(hr)) goto lerror;
	}

	poGraph = pGraph;
	poSplitter = pSplitter;
	return S_OK;
lerror:
	return E_FAIL;
}

void SSIFSource::ParseEvents() {
	if (!pGraph)
		return;

	CComQIPtr<IMediaEventEx> pEvent = pGraph;
	long evCode = 0;
	LONG_PTR param1 = 0, param2 = 0;
	HRESULT hr = S_OK;
	while (SUCCEEDED(pEvent->GetEvent(&evCode, &param1, &param2, 0))) {
		// Invoke the callback.
		if (evCode == EC_COMPLETE) {
			CComQIPtr<IMediaControl>(pGraph)->Stop();
			pSplitter = NULL;
			pGraph = NULL;
		}

		// Free the event data.
		hr = pEvent->FreeEventParams(evCode, param1, param2);
		if (FAILED(hr))
			break;
	}
}

void SSIFSource::InitVariables() {
	// VideoType
    frame_vi.width = data.dim_width;
    frame_vi.height = data.dim_height;
	frame_vi.fps_numerator = 24000;
	frame_vi.fps_denominator = 1001;
	frame_vi.pixel_type = VideoInfo::CS_I420;
	frame_vi.num_frames = data.frame_count;
    frame_vi.audio_samples_per_second = 0;

    vi = frame_vi;
	if ((data.show_params & (SP_LEFTVIEW | SP_RIGHTVIEW)) == (SP_LEFTVIEW | SP_RIGHTVIEW))
		((data.show_params & SP_HORIZONTAL) ? vi.width : vi.height) *= 2;
	
	last_frame = FRAME_BLACK;
	
#ifdef _DEBUG
	AllocConsole();
#endif
	memset(&SI, 0, sizeof(STARTUPINFO));
	SI.cb = sizeof(SI);
	SI.dwFlags = STARTF_USESHOWWINDOW | STARTF_FORCEOFFFEEDBACK;
#ifdef _DEBUG
	SI.wShowWindow = SW_SHOWNORMAL;
#else
	SI.wShowWindow = SW_HIDE;
#endif

	memset(&PI1, 0, sizeof(PROCESS_INFORMATION));
	PI1.hProcess = INVALID_HANDLE_VALUE;
	memset(&PI2, 0, sizeof(PROCESS_INFORMATION));
	PI2.hProcess = INVALID_HANDLE_VALUE;

	frLeft = NULL;
    frRight = NULL;
	dupThread1 = NULL;
	unic_number = rand();

	pipes_over_warning = false;
}

void SSIFSource::InitDemuxer() {
	USES_CONVERSION;
	string
		fnBase = MakePipeName(unic_number, "base.h264"),
		fnDept = MakePipeName(unic_number, "dept.h264");
	data.left_264 = MakePipeName(unic_number, "base_merge.h264");
	data.right_264 = MakePipeName(unic_number, "dept_merge.h264");

	dupThread2 = new PipeDupThread(fnBase.c_str(), data.left_264.c_str());
	if (data.show_params & SP_RIGHTVIEW)
		dupThread3 = new PipeDupThread(fnDept.c_str(), data.right_264.c_str());

	CoInitialize(NULL);
	HRESULT res = CreateGraph(
		A2W(data.ssif_file.c_str()),
		A2W(fnBase.c_str()),
		(data.show_params & SP_RIGHTVIEW) ? A2W(fnDept.c_str()): NULL,
		pGraph, pSplitter);
	if (FAILED(res))
		throw (string)"Error creating graph. Code: " + IntToStr(res);

	res = CComQIPtr<IMediaControl>(pGraph)->Run();
	if (FAILED(res))
		throw(string)"Can't start the graph";
	ParseEvents();
}

void SSIFSource::InitMuxer() {
	data.h264muxed = MakePipeName(unic_number, "intel_input.h264");

	string
		name_muxer = program_path + PATH_MERGE "merge.exe",
		s_muxer_output_write = MakePipeName(unic_number, "muxed.h264"),
		cmd_muxer = "\"" + name_muxer + "\" "
			"\"" + data.left_264 + "\" " +
			"\"" + data.right_264 + "\" " +
			"\"" + s_muxer_output_write + "\" ";

	dupThread1 = new PipeDupThread(s_muxer_output_write.c_str(), data.h264muxed.c_str());
	if (!CreateProcessA(name_muxer.c_str(), const_cast<char*>(cmd_muxer.c_str()), NULL, NULL, false, 
			CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED, NULL, NULL, &SI, &PI1))
	{
		throw (string)"Error while launching " + name_muxer + "\n";
	}
}

void SSIFSource::InitDecoder() {
	bool flag_mvc = (data.show_params & SP_RIGHTVIEW) != 0;
    string
		name_decoder = program_path + PATH_DECODER "sample_decode.exe",
		s_dec_left_write = MakePipeName(unic_number, "output_0.yuv"),
		s_dec_right_write = MakePipeName(unic_number, "output_1.yuv"),
		s_dec_out = MakePipeName(unic_number, "output"),
		cmd_decoder = "\"" + name_decoder + "\" " +
			(flag_mvc ? "mvc" : "h264") +
			" -hw -d3d11 " +
			" -i \"" + data.h264muxed + "\" -o " + s_dec_out;

	int framesize = frame_vi.width*frame_vi.height + (frame_vi.width*frame_vi.height/2)&~1;
	if (!flag_mvc)
		s_dec_left_write = s_dec_out;

	frLeft = new FrameSeparator(s_dec_left_write.c_str(), framesize);
	if (flag_mvc)
		frRight = new FrameSeparator(s_dec_right_write.c_str(), framesize);

	if (!CreateProcessA(name_decoder.c_str(), const_cast<char*>(cmd_decoder.c_str()), NULL, NULL, false,
			CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED, NULL, NULL, &SI, &PI2))
	{
		throw (string)"Error while launching " + name_decoder + "\n";
	}
}

void SSIFSource::InitComplete() {
	ResumeThread(PI1.hThread);
	ResumeThread(PI2.hThread);
	last_frame = FRAME_START;
}

SSIFSource::SSIFSource(IScriptEnvironment* env, const SSIFSourceParams& data): data(data) {
	InitVariables();

	try {
		InitDemuxer();
		InitMuxer();
		InitDecoder();
		InitComplete();
	}
	catch(const string& obj) {
		env->ThrowError(string(FILTER_NAME ": " + obj).c_str());
	}
}

SSIFSource::~SSIFSource() {
	if (PI2.hProcess != INVALID_HANDLE_VALUE)
		TerminateProcess(PI2.hProcess, 0);
	if (PI1.hProcess != INVALID_HANDLE_VALUE)
		TerminateProcess(PI1.hProcess, 0);
	if (dupThread1 != NULL)
		delete dupThread1;
	if (dupThread2 != NULL)
		delete dupThread2;
	if (dupThread3 != NULL)
		delete dupThread3;
	if (frLeft != NULL) 
		delete frLeft;
	if (frRight != NULL)
		delete frRight;
	pSplitter = NULL;
	pGraph = NULL;
	CoUninitialize();
}

PVideoFrame SSIFSource::ReadFrame(FrameSeparator* frSep, IScriptEnvironment* env) {
    PVideoFrame res = env->NewVideoFrame(frame_vi);
    if (!frSep->error) {
        frSep->WaitForData();
    } else {
        if (!pipes_over_warning) {
            fprintf(stderr, "\nWARNING: Decoder output finished. Frame separator can't read next frames. "
                "Last frame will be duplicated as long as necessary (%d time(s)).\n", vi.num_frames-last_frame);
            pipes_over_warning = true;
        }
    }
    memcpy(res->GetWritePtr(), (char*)frSep->buffer, frSep->size);
    frSep->DataParsed();
    return res;
}

void SSIFSource::DropFrame(FrameSeparator* frSep) {
    if (!frSep->error)
        frSep->WaitForData();
    frSep->DataParsed();
}

PVideoFrame WINAPI SSIFSource::GetFrame(int n, IScriptEnvironment* env) {
	ParseEvents();

	if (last_frame+1 != n || n >= vi.num_frames) {
		string str = "ERROR:\\n"
			"Can't retrieve frame #" + IntToStr(n) + " !\\n";
		if (last_frame >= vi.num_frames-1)
			str += "Video sequence is over. Reload the script.\\n";
		else
			str += "Frame #" + IntToStr(last_frame+1) + " should be the next frame.\\n";
		str += "Note: " FILTER_NAME " filter supports only sequential frame rendering.";
		const char* arg_names1[2] = {0, "color_yuv"};
		AVSValue args1[2] = {this, 0};
		PClip resultClip1 = (env->Invoke("BlankClip", AVSValue(args1,2), arg_names1)).AsClip();

		const char* arg_names2[6] = {0, 0, "lsp", "size", "text_color", "halo_color"};
		AVSValue args2[6] = {resultClip1, AVSValue(str.c_str()), 10, 50, 0xFFFFFF, 0x000000};
		PClip resultClip2 = (env->Invoke("Subtitle", AVSValue(args2,6), arg_names2)).AsClip();
		return resultClip2->GetFrame(n, env);
	}
	last_frame = n;
    
    PVideoFrame left, right;
    if (data.show_params & SP_LEFTVIEW) 
        left = ReadFrame(frLeft, env);
    else
        DropFrame(frLeft);
    if (data.show_params & SP_RIGHTVIEW) 
        right = ReadFrame(frRight, env);

	if ((data.show_params & (SP_LEFTVIEW|SP_RIGHTVIEW)) == (SP_LEFTVIEW|SP_RIGHTVIEW)) {
		if (!(data.show_params & SP_SWAPVIEWS))
			return FrameStack(env, frame_vi, left, right, (data.show_params & SP_HORIZONTAL) != 0);
		else
			return FrameStack(env, frame_vi, right, left, (data.show_params & SP_HORIZONTAL) != 0);
	}
    else if (data.show_params & SP_LEFTVIEW)
        return left;
    else
        return right;
}

AVSValue __cdecl Create_SSIFSource(AVSValue args, void* user_data, IScriptEnvironment* env) {
	SSIFSourceParams data;

	data.ssif_file = args[0].AsString();
	data.frame_count = args[1].AsInt();
	data.dim_width = 1920;
	data.dim_height = 1080;
	data.show_params = 
		(args[2].AsBool(true) ? SP_LEFTVIEW : 0) |
		(args[3].AsBool(true) ? SP_RIGHTVIEW : 0) |
		(args[4].AsBool(false) ? SP_HORIZONTAL : 0) |
		(args[5].AsBool(false) ? SP_SWAPVIEWS : 0);
	if (!(data.show_params & (SP_LEFTVIEW | SP_RIGHTVIEW))) {
        env->ThrowError(FILTER_NAME ": can't show nothing");
    }
	return new SSIFSource(env, data);
}