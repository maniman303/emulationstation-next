#pragma once

#include <mutex>

namespace StatusComponents
{
	// Watchers publish readings; UI preferences never modify the source value.
	template <typename Value>
	class LatestReading
	{
	public:
		Value get() const
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return mValue;
		}

		void set(const Value& value)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mValue = value;
			mHasValue = true;
		}

		// Subscribe before seeding. A notification received during construction
		// must take precedence over the possibly older initial snapshot.
		void setInitial(const Value& value)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			if (!mHasValue)
			{
				mValue = value;
				mHasValue = true;
			}
		}

	private:
		mutable std::mutex mMutex;
		Value mValue{};
		bool mHasValue = false;
	};

	// Shared by text and image status components in ordinary and covered updates.
	// The presenter also reports asynchronous image readiness when applicable.
	template <typename Component, typename Present>
	bool refreshPresentation(Component& component, Present present)
	{
		const bool visible = component.isVisible();
		const auto value = component.getValue();
		const auto size = component.getSize();
		const bool readinessChanged = present();
		return readinessChanged || visible != component.isVisible() ||
			value != component.getValue() || size != component.getSize();
	}
}
