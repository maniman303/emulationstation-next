#pragma once

class CoveredSceneRenderScope
{
public:
	CoveredSceneRenderScope();
	~CoveredSceneRenderScope();

	static bool isActive();

private:
	bool mPrevious;
	static bool sActive;
};
