#pragma once
#include "Tools.AviSynth.hpp"
#include "Tools.Pipe.hpp"
#include "Tools.WinApi.hpp"

namespace Filter {

	using namespace Tools::Pipe;
	using namespace Tools::WinApi;

	class mplsSource2 : public Tools::AviSynth::SourceFilterStub {
	public:
		mplsSource2(IScriptEnvironment* env, AVSValue args);

		PVideoFrame WINAPI GetFrame(int n, IScriptEnvironment* env) override;

		static AvsParams CreateParams;
		static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);

	private:
		PClip currentClip;
		std::string ssifPath;
		bool flagMVC;
		bool flagSwapViews;
		std::unique_ptr<ProxyThread> proxyLeft, proxyRight;
		std::unique_ptr<ProcessHolder> process;
		UniqueIdHolder uniqueId;
	};


}
