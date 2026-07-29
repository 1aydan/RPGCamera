#include "TopDownRPGCameraModule.h"

DEFINE_LOG_CATEGORY(LogTopDownRPGCamera);

#define LOCTEXT_NAMESPACE "FTopDownRPGCameraModule"

void FTopDownRPGCameraModule::StartupModule()
{
}

void FTopDownRPGCameraModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTopDownRPGCameraModule, TopDownRPGCamera)
