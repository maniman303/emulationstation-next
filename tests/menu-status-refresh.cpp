#include "BackgroundStatusRefresh.h"
#include "CoveredSceneRenderScope.h"
#include "StatusComponentState.h"
#include "utils/Platform.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

struct TestComponent
{
	TestComponent(bool isDesignated, bool isVisible, bool hasChanged) : designated(isDesignated), visible(isVisible), changed(hasChanged) {}
	virtual ~TestComponent() = default;
	virtual bool refreshBackgroundStatus()
	{
		return refreshBackgroundStatusTree(this,
			[](TestComponent* component) {
				++component->statusVisits;
				return component->designated && component->changed;
			},
			[](TestComponent* component) -> const std::vector<TestComponent*>& { return component->children; },
			[](TestComponent* child) { return child->refreshBackgroundStatus(); });
	}

	bool designated;
	bool visible;
	bool changed;
	unsigned int statusVisits = 0;
	unsigned int ordinaryUpdates = 0;
	std::vector<TestComponent*> children;
};

struct TestOwner : TestComponent
{
	TestOwner(TestComponent* ownedChild) : TestComponent(false, true, false), owned(ownedChild) {}
	bool refreshBackgroundStatus() override
	{
		++overrideVisits;
		const bool changed = TestComponent::refreshBackgroundStatus();
		return owned->refreshBackgroundStatus() || changed;
	}
	TestComponent* owned;
	unsigned int overrideVisits = 0;
};

static unsigned int checks = 0;

static void check(bool condition, const char* message)
{
	++checks;
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

static void testCadence()
{
	int root = 0;
	BackgroundStatusRefresh refresh;
	unsigned int updates = 0;
	for (uint32_t now = 0; now < 10000; now += 16)
		if (refresh.observe(true, &root, now))
			++updates;

	check(updates >= 90 && updates <= 110, "100 ms cadence stays within tolerance under 16 ms frame rounding");
	check(refresh.getTimeout(10000) >= 0, "active policy exposes a finite refresh timeout");

	refresh.reset();
	check(refresh.getTimeout(0) == -1, "inactive policy has no deadline");
	check(refresh.observe(true, &root, UINT32_MAX - 49), "eligible coverage refreshes immediately near tick wrap");
	check(refresh.getTimeout(UINT32_MAX - 49) == 100, "first interval remains 100 ms near tick wrap");
	check(!refresh.observe(true, &root, 25), "deadline comparison wraps safely before next interval");
	check(refresh.observe(true, &root, 50), "deadline comparison wraps safely at next interval");
	check(!refresh.observe(true, &root, 125), "a missed deadline does not trigger a catch-up pass");
	check(refresh.observe(true, &root, 225), "next opportunity follows a long stall by one interval");
	check(!refresh.observe(true, &root, 226), "long stall does not produce a burst");
	check(!refresh.observe(false, &root, 300), "inactive coverage resets the cadence");
	check(refresh.observe(true, &root, 301), "re-entering coverage receives a fresh immediate pass");
	int newRoot = 0;
	check(refresh.observe(true, &newRoot, 302), "replacing the covered root resets timing immediately");
}

static void testStatusTraversal()
{
	TestComponent root{false, true, false};
	TestComponent hiddenStatus{true, false, true};
	TestComponent decoration{false, true, false};
	TestComponent nestedStatus{true, true, false};
	root.children = {&hiddenStatus, &decoration};
	decoration.children = {&nestedStatus};

	auto changed = root.refreshBackgroundStatus();

	check(changed, "status traversal reports a hidden descendant presentation change");
	check(root.statusVisits == 1 && hiddenStatus.statusVisits == 1 && decoration.statusVisits == 1 && nestedStatus.statusVisits == 1,
		"status traversal visits every node exactly once without visibility filtering");
	check(root.ordinaryUpdates == 0 && hiddenStatus.ordinaryUpdates == 0 && decoration.ordinaryUpdates == 0 && nestedStatus.ordinaryUpdates == 0,
		"status traversal does not invoke ordinary updates");
	hiddenStatus.changed = false;
	changed = root.refreshBackgroundStatus();
	check(!changed, "stable status traversal reports no visual change");
	check(hiddenStatus.statusVisits == 2 && nestedStatus.statusVisits == 2,
		"stable traversal does not short-circuit later status instances");

	TestComponent ownedStatus{true, false, true};
	TestOwner owner(&ownedStatus);
	decoration.children.push_back(&owner);
	check(root.refreshBackgroundStatus(), "nested ownership overrides contribute presentation changes");
	check(owner.overrideVisits == 1 && owner.statusVisits == 1 && ownedStatus.statusVisits == 1,
		"nested containers dispatch their overrides and owned descendants exactly once");
	ownedStatus.visible = true;
	check(root.refreshBackgroundStatus() && ownedStatus.statusVisits == 2,
		"a hidden status instance remains reachable when it becomes visible again");
}

static void testRenderScope()
{
	check(!CoveredSceneRenderScope::isActive(), "render scope starts inactive");
	{
		CoveredSceneRenderScope outer;
		check(CoveredSceneRenderScope::isActive(), "render scope becomes active");
		{
			CoveredSceneRenderScope inner;
			check(CoveredSceneRenderScope::isActive(), "nested render scope remains active");
		}
		check(CoveredSceneRenderScope::isActive(), "nested scope restores prior active state");
	}
	check(!CoveredSceneRenderScope::isActive(), "outer scope restores inactive state");
}

struct StatusPresentationFixture
{
	bool visible = true;
	std::string value = "75%";
	int size = 20;
	bool isVisible() const { return visible; }
	std::string getValue() const { return value; }
	int getSize() const { return size; }
};

static void testStatusPresentation()
{
	StatusComponents::LatestReading<Utils::Platform::BatteryInformation> source;
	Utils::Platform::BatteryInformation reading;
	reading.hasBattery = true;
	reading.level = 75;
	source.set(reading);
	StatusPresentationFixture component;
	bool enabled = true;
	auto refresh = [&]() {
		return StatusComponents::refreshPresentation(component, [&]() {
			const auto latest = source.get();
			component.visible = enabled && latest.hasBattery && latest.level >= 0;
			if (component.visible)
				component.value = std::to_string(latest.level) + "%";
			return false;
		});
	};
	check(!refresh(), "unchanged readings and preferences retain the presented state");
	enabled = false;
	check(refresh() && !component.visible, "preference-only disable hides without a watcher event");
	check(!refresh(), "stable hidden status does not repeatedly invalidate presentation");
	check(source.get().hasBattery && source.get().level == 75, "hiding status preserves the source reading");
	enabled = true;
	check(refresh() && component.visible, "preference-only re-enable restores the existing reading");
	reading.hasBattery = false;
	source.set(reading);
	check(refresh() && !component.visible, "missing hardware hides its presentation");
	reading.hasBattery = true;
	source.set(reading);
	check(refresh() && component.visible, "hardware returning restores the hidden status");
	reading.level = -1;
	source.set(reading);
	check(refresh() && !component.visible, "unavailable percentage is hidden");
	reading.level = 81;
	source.set(reading);
	check(refresh() && component.visible && component.value == "81%", "valid reading returns after unavailable data");
	enabled = false;
	refresh();
	reading.level = 82;
	source.set(reading);
	check(!refresh(), "hidden reading changes do not unnecessarily invalidate presentation");
	enabled = true;
	check(refresh() && component.value == "82%", "re-enable presents the newest reading received while hidden");
	check(StatusComponents::refreshPresentation(component, [&]() { component.size = 30; return false; }),
		"dimension-only status changes invalidate presentation and can trigger row repacking");
	check(StatusComponents::refreshPresentation(component, []() { return true; }),
		"asynchronous image readiness invalidates even with unchanged value and dimensions");
	check(!StatusComponents::refreshPresentation(component, []() { return false; }),
		"unchanged ready status retains its cache");
}

static void testLatestReading()
{
	struct Reading { int version = 0; int inverse = 0; };
	StatusComponents::LatestReading<Reading> source;
	check(source.get().version == 0, "status starts with a safe default before the first watcher reading");
	source.setInitial(Reading{1, -1});
	check(source.get().version == 1, "an initial snapshot seeds a component before any notification");
	source.set(Reading{2, -2});
	source.setInitial(Reading{1, -1});
	check(source.get().version == 2, "an older initial read cannot overwrite a newer watcher notification");
	std::atomic<bool> done(false);
	std::thread producer([&]() {
		for (int i = 1; i <= 10000; ++i)
			source.set(Reading{i, -i});
		done.store(true);
	});
	bool consistent = true;
	do
	{
		const auto value = source.get();
		consistent &= value.version == -value.inverse;
	} while (!done.load());
	producer.join();
	check(consistent, "watcher and UI threads exchange complete snapshots rather than torn readings");
	check(source.get().version == 10000, "the final watcher reading remains available without a dirty event");
	auto local = source.get();
	local.version = 0;
	check(source.get().version == 10000, "presentation changes to a local copy cannot overwrite the source");
}

int main()
{
	testCadence();
	testStatusTraversal();
	testRenderScope();
	testStatusPresentation();
	testLatestReading();
	std::cout << checks << " menu status refresh checks passed\n";
	return 0;
}
