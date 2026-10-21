#include "CoveredSceneRenderScope.h"

bool CoveredSceneRenderScope::sActive = false;

CoveredSceneRenderScope::CoveredSceneRenderScope() : mPrevious(sActive)
{
	sActive = true;
}

CoveredSceneRenderScope::~CoveredSceneRenderScope()
{
	sActive = mPrevious;
}

bool CoveredSceneRenderScope::isActive()
{
	return sActive;
}
