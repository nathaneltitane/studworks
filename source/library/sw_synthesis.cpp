#include "sw_global.h"
#include "sw_synthesis.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_file.h"
#include "sw_loader_mesh.h"
#include "sw_mesh.h"
#include "sw_data_part.h"
#include "sw_parser_shadow.h"
#include <locale.h>

class swSynthesisInformationCurved : public swSynthesisInformation
{
public:
	swSynthesisInformationCurved(float Length, float DefaultScale, int NumSections, bool RigidEdges);

	void GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;
	void VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;
	swMatrix33 GetSectionControlPointRotation(const swMatrix44& Section, const swMatrix33& Rotation) const override;
	bool IsReachable(const std::vector<swPartControlPoint>& ControlPoints) const override;
	void GetPath(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swVector3>& Points, size_t& ReachedPointCount) const override;

protected:
	float GetSectionTwist(const swMatrix44& StartTransform, const swMatrix44& EndTransform) const;
	float GetSectionSpacing(size_t SectionCount) const;
	float GetNominalLength() const;
	float GetEndInset() const;
	float GetHandleDirection(const std::vector<swPartControlPoint>& ControlPoints, quint32 ControlPointIndex) const;
	void GetSegmentControlPoints(const std::vector<swPartControlPoint>& ControlPoints, const std::vector<float>& Scales, quint32 ControlPointIndex, swVector3 SegmentControlPoints[4], swMatrix44& StartTransform, swMatrix44& EndTransform) const;
	float GetPathLength(const std::vector<swPartControlPoint>& ControlPoints, const std::vector<float>& Scales, int CurvePointCount, float* BendEnergy) const;
	float FitPathLength(const std::vector<swPartControlPoint>& ControlPoints, std::vector<float>& Scales, float NominalLength, float* BendEnergy) const;
	std::vector<float> GetFittedScales(const std::vector<swPartControlPoint>& ControlPoints, float NominalLength, const std::atomic_bool* Cancelled) const;
	void CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
	static void AddTubeParts(swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, float Radius, bool IsInner, const std::atomic_bool* Cancelled);

	struct swSynthesisComponent
	{
		swMatrix44 Transform;
		float Length;
	};

	swSynthesisComponent mStart;
	swSynthesisComponent mMiddle;
	swSynthesisComponent mEnd;
	float mCenterLength = 0.0f;
	size_t mSectionCount;
	float mDefaultScale;
	bool mRigidEdges;
};

class swSynthesisInformationFlexibleHose : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationFlexibleHose(float Length, int NumSections, const char* EdgePart2);

protected:
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mEdgePart2;
};

class swSynthesisInformationFlexSystemHose : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationFlexSystemHose(float Length, int NumSections);

	void GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;

protected:
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationPneumaticTube : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationPneumaticTube(float Length, int NumSections, const char* EndPart);

protected:
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mEndPart;
};

class swSynthesisInformationRibbedHose : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationRibbedHose(float Length, int NumSections);

protected:
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationFlexibleAxle : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationFlexibleAxle(float Length, int NumSections);

protected:
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationBraidedString : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationBraidedString(float Length, int NumSections);

protected:
	void CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationStraight : public swSynthesisInformation
{
public:
	explicit swSynthesisInformationStraight(float Length);

	void VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;

protected:
	void CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationShockAbsorber : public swSynthesisInformationStraight
{
public:
	explicit swSynthesisInformationShockAbsorber(const char* SpringPart);

	void GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;

protected:
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mSpringPart;
};

class swSynthesisInformationActuator : public swSynthesisInformationStraight
{
public:
	explicit swSynthesisInformationActuator(const char* BodyPart, const char* PistonPart, const char* AxlePart, float Length, float AxleOffset);

	void GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;

protected:
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mBodyPart;
	const char* mPistonPart;
	const char* mAxlePart;
	float mAxleOffset;
};

class swSynthesisInformationUniversalJoint : public swSynthesisInformation
{
public:
	swSynthesisInformationUniversalJoint(float Length, float EndOffset, const char* EndPart, const char* CenterPart);

	void GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;
	void VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const override;

protected:
	void CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
	void AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	float mEndOffset;
	const char* mEndPart;
	const char* mCenterPart;
};

void swSynthesisInitialize()
{
	swPartsLibrary* Library = swGetPartsLibrary();

	static const struct
	{
		char PartID[16];
		float Length;
		int NumSections;
		char EdgePart2[8];
	}
	FlexibleHoses[] =
	{
		{ "73590a.dat", 140.0f, 51, "752.dat" }, // Hose Flexible  8.5L without Tabs
		{ "73590b.dat", 140.0f, 51, "750.dat" }, // Hose Flexible  8.5L with Tabs
	};

	for (const auto& HoseInfo: FlexibleHoses)
	{
		swPartData* PartData = Library->FindPartData(HoseInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationFlexibleHose(HoseInfo.Length, HoseInfo.NumSections, HoseInfo.EdgePart2));
	}

	static const struct
	{
		char PartID[16];
		float Length;
		int NumSections;
	}
	FlexSystemHoses[] =
	{
		{ "76263.dat",      60.0f,  29 }, // Technic Flex-System Hose  3L (60LDU)
		{ "76250.dat",      80.0f,  39 }, // Technic Flex-System Hose  4L (80LDU)
		{ "76307.dat",     100.0f,  49 }, // Technic Flex-System Hose  5L (100LDU)
		{ "76279.dat",     120.0f,  59 }, // Technic Flex-System Hose  6L (120LDU)
		{ "76289.dat",     140.0f,  69 }, // Technic Flex-System Hose  7L (140LDU)
		{ "76260.dat",     160.0f,  79 }, // Technic Flex-System Hose  8L (160LDU)
		{ "76324.dat",     180.0f,  89 }, // Technic Flex-System Hose  9L (180LDU)
		{ "76348.dat",     200.0f,  99 }, // Technic Flex-System Hose 10L (200LDU)
		{ "71505.dat",     220.0f, 109 }, // Technic Flex-System Hose 11L (220LDU)
		{ "71175.dat",     240.0f, 119 }, // Technic Flex-System Hose 12L (240LDU)
		{ "71551.dat",     260.0f, 129 }, // Technic Flex-System Hose 13L (260LDU)
		{ "71177.dat",     280.0f, 139 }, // Technic Flex-System Hose 14L (280LDU)
		{ "71194.dat",     300.0f, 149 }, // Technic Flex-System Hose 15L (300LDU)
		{ "71192.dat",     320.0f, 159 }, // Technic Flex-System Hose 16L (320LDU)
		{ "76270.dat",     340.0f, 169 }, // Technic Flex-System Hose 17L (340LDU)
		{ "71582.dat",     360.0f, 179 }, // Technic Flex-System Hose 18L (360LDU)
		{ "22463.dat",     380.0f, 189 }, // Technic Flex-System Hose 19L (380LDU)
		{ "76276.dat",     400.0f, 199 }, // Technic Flex-System Hose 20L (400LDU)
		{ "70978.dat",     420.0f, 209 }, // Technic Flex-System Hose 21L (420LDU)
		{ "76252.dat",     440.0f, 219 }, // Technic Flex-System Hose 22L (440LDU)
		{ "76254.dat",     460.0f, 229 }, // Technic Flex-System Hose 23L (460LDU)
		{ "76277.dat",     480.0f, 239 }, // Technic Flex-System Hose 24L (480LDU)
		{ "53475.dat",     520.0f, 259 }, // Technic Flex-System Hose 26L (520LDU)
		{ "76280.dat",     560.0f, 279 }, // Technic Flex-System Hose 28L (560LDU)
		{ "76389.dat",     580.0f, 289 }, // Technic Flex-System Hose 29L (580LDU)
		{ "76282.dat",     600.0f, 299 }, // Technic Flex-System Hose 30L (600LDU)
		{ "76283.dat",     620.0f, 309 }, // Technic Flex-System Hose 31L (620LDU)
		{ "57274.dat",     640.0f, 319 }, // Technic Flex-System Hose 32L (640LDU)
		{ "42688.dat",     660.0f, 329 }, // Technic Flex-System Hose 33L (660LDU)
		{ "22461.dat",     680.0f, 339 }, // Technic Flex-System Hose 34L (680LDU)
		{ "46305.dat",     800.0f, 399 }, // Technic Flex-System Hose 40L (800LDU)
		{ "76281.dat",     900.0f, 449 }, // Technic Flex-System Hose 45L (900LDU)
		{ "22296.dat",    1060.0f, 529 }, // Technic Flex-System Hose 53L (1060LDU)
	};

	for (const auto& HoseInfo: FlexSystemHoses)
	{
		swPartData* PartData = Library->FindPartData(HoseInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationFlexSystemHose(HoseInfo.Length, HoseInfo.NumSections));
	}

	static const struct
	{
		char PartID[16];
		float Length;
		int NumSections;
	}
	PneumaticTubes[] =
	{
		{ "21761-f1.dat",   60.0f,  10 }, // Technic Pneumatic Tube  3L
		{ "26445-f1.dat",   80.0f,  20 }, // Technic Pneumatic Tube  4L
		{ "14653-f1.dat",  100.0f,  20 }, // Technic Pneumatic Tube  5L
		{ "21766-f1.dat",  120.0f,  40 }, // Technic Pneumatic Tube  6L
		{ "14657-f1.dat",  140.0f,  40 }, // Technic Pneumatic Tube  7L
		{ "21837-f1.dat",  160.0f,  40 }, // Technic Pneumatic Tube  8L
		{ "21826-f1.dat",  180.0f,  40 }, // Technic Pneumatic Tube  9L
		{ "21767-f1.dat",  200.0f,  80 }, // Technic Pneumatic Tube 10L
		{ "63539-f1.dat",  240.0f,  80 }, // Technic Pneumatic Tube 12L
		{ "37467-f1.dat",  260.0f,  80 }, // Technic Pneumatic Tube 13L
		{ "37461-f1.dat",  280.0f,  80 }, // Technic Pneumatic Tube 14L
		{ "87948-f1.dat",  320.0f,  80 }, // Technic Pneumatic Tube 16L
		{ "53168-f1.dat",  340.0f,  80 }, // Technic Pneumatic Tube 17L
		{ "21839-f1.dat",  380.0f, 160 }, // Technic Pneumatic Tube 19L
		{ "87950-f1.dat",  400.0f, 160 }, // Technic Pneumatic Tube 20L
		{ "53184-f1.dat",  420.0f, 160 }, // Technic Pneumatic Tube 21L
		{ "26436-f1.dat",  460.0f, 160 }, // Technic Pneumatic Tube 23L
		{ "21830-f1.dat",  540.0f, 160 }, // Technic Pneumatic Tube 27L
		{ "21825-f1.dat",  560.0f, 160 }, // Technic Pneumatic Tube 28L
		{ "21833-f1.dat",  600.0f, 160 }, // Technic Pneumatic Tube 30L
		{ "26440-f1.dat",  640.0f, 160 }, // Technic Pneumatic Tube 32L
		{ "96889-f1.dat",  660.0f, 160 }, // Technic Pneumatic Tube 33L
		{ "87949-f1.dat",  720.0f, 320 }, // Technic Pneumatic Tube 36L
		{ "21835-f1.dat",  740.0f, 320 }, // Technic Pneumatic Tube 37L
		{ "14661-f1.dat",  780.0f, 320 }, // Technic Pneumatic Tube 39L
		{ "26438-f1.dat",  800.0f, 320 }, // Technic Pneumatic Tube 40L
		{ "44079-f1.dat",  840.0f, 320 }, // Technic Pneumatic Tube 42L
		{ "26439-f1.dat",  960.0f, 320 }, // Technic Pneumatic Tube 48L
		{ "96890-f1.dat", 1080.0f, 320 }, // Technic Pneumatic Tube 54L
		{ "96891-f1.dat", 1600.0f, 320 }, // Technic Pneumatic Tube 80L
	};

	for (const auto& TubeInfo: PneumaticTubes)
	{
		swPartData* PartData = Library->FindPartData(TubeInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationPneumaticTube(TubeInfo.Length, TubeInfo.NumSections, "71533k02.dat"));

		auto RegularInfo = TubeInfo;
		RegularInfo.PartID[7] = '2';
		RegularInfo.Length -= 40.0f;

		PartData = Library->FindPartData(RegularInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationPneumaticTube(RegularInfo.Length, RegularInfo.NumSections, "71533k01.dat"));
	}

	static const struct
	{
		char PartID[16];
		float Length;
		int NumSections;
	}
	RibbedHoses[] =
	{
		{ "72504.dat",     31.25f,   4 }, // Technic Ribbed Hose  2L
		{ "72706.dat",     50.00f,   7 }, // Technic Ribbed Hose  3L
		{ "71952.dat",     75.00f,  11 }, // Technic Ribbed Hose  4L
		{ "72853.dat",     93.75f,  14 }, // Technic Ribbed Hose  5L
		{ "71944.dat",    112.50f,  17 }, // Technic Ribbed Hose  6L
		{ "57719.dat",    131.25f,  20 }, // Technic Ribbed Hose  7L
		{ "71951.dat",    150.00f,  23 }, // Technic Ribbed Hose  8L
		{ "71917.dat",    175.00f,  27 }, // Technic Ribbed Hose  9L
		{ "71949.dat",    193.75f,  30 }, // Technic Ribbed Hose 10L
		{ "71986.dat",    212.50f,  33 }, // Technic Ribbed Hose 11L
		{ "71819.dat",    231.25f,  36 }, // Technic Ribbed Hose 12L
		{ "71923.dat",    275.00f,  43 }, // Technic Ribbed Hose 14L
		{ "71946.dat",    293.75f,  46 }, // Technic Ribbed Hose 15L
		{ "71947.dat",    312.50f,  49 }, // Technic Ribbed Hose 16L
		{ "22900.dat",    331.25f,  52 }, // Technic Ribbed Hose 17L
		{ "72039.dat",    350.00f,  55 }, // Technic Ribbed Hose 18L
		{ "43675.dat",    375.00f,  58 }, // Technic Ribbed Hose 19L
		{ "23397.dat",    468.75f,  74 }, // Technic Ribbed Hose 24L
		{ "33763.dat",    512.50f,  81 }, // Technic Ribbed Hose 26L
	};

	for (const auto& HoseInfo: RibbedHoses)
	{
		swPartData* PartData = Library->FindPartData(HoseInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationRibbedHose(HoseInfo.Length, HoseInfo.NumSections));
	}

	static const struct
	{
		char PartID[16];
		float Length;
		int NumSections;
	}
	FlexibleAxles[] =
	{
		{ "32580.dat",    120.00f,  15 }, // Technic Axle Flexible  7
		{ "32199.dat",    200.00f,  35 }, // Technic Axle Flexible 11
		{ "55709.dat",    200.00f,  35 }, // Technic Axle Flexible 11
		{ "32200.dat",    220.00f,  40 }, // Technic Axle Flexible 12
		{ "32201.dat",    260.00f,  50 }, // Technic Axle Flexible 14
		{ "32202.dat",    300.00f,  60 }, // Technic Axle Flexible 16
		{ "32235.dat",    360.00f,  75 }, // Technic Axle Flexible 19
	};

	for (const auto& AxleInfo: FlexibleAxles)
	{
		swPartData* PartData = Library->FindPartData(AxleInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationFlexibleAxle(AxleInfo.Length, AxleInfo.NumSections));
	}

	static const struct
	{
		char PartID[16];
		float Length;
		int NumSections;
	}
	BraidedStrings[] =
	{
		{ "76384.dat",    200.00f,  46 }, // String Braided 11L with End Studs
		{ "75924.dat",    400.00f,  96 }, // String Braided 21L with End Studs
		{ "572C02.dat",   800.00f, 196 }, // String Braided 41L with End Studs
	};

	for (const auto& StringInfo: BraidedStrings)
	{
		swPartData* PartData = Library->FindPartData(StringInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationBraidedString(StringInfo.Length, StringInfo.NumSections));
	}

	static const struct
	{
		char PartID[16];
		char SpringPart[16];
	}
	ShockAbsorbers[] =
	{
		{ "73129.dat", "70038.dat" }, // Technic Shock Absorber 6.5L
		{ "41838.dat", "41837.dat" }, // Technic Shock Absorber 6.5L Soft
		{ "76138.dat", "71953.dat" }, // Technic Shock Absorber 6.5L Stiff
		{ "76537.dat", "22977.dat" }, // Technic Shock Absorber 6.5L Extra Stiff
	};

	for (const auto& AbsorberInfo: ShockAbsorbers)
	{
		swPartData* PartData = Library->FindPartData(AbsorberInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationShockAbsorber(AbsorberInfo.SpringPart));
	}

	static const struct
	{
		char PartID[16];
		char BodyPart[16];
		char PistonPart[16];
		char AxlePart[16];
		float Length;
		float AxleOffset;
	}
	Actuators[] =
	{
		{ "61927-F1.dat",    "62271c01.dat", "62274c01.dat", "47157.dat", 170.00f,  0.0f }, // Technic Linear Actuator 8 x 2 x 2 (Contracted)
		{ "61927-F2.dat",    "62271c01.dat", "62274c01.dat", "47157.dat", 270.00f,  0.0f }, // Technic Linear Actuator 8 x 2 x 2 (Extended)
		{ "61927C01.dat",    "62271c01.dat", "62274c01.dat", "47157.dat", 270.00f,  0.0f }, // Moved to 61927-f2 (was Technic Power Functions Linear Actuator (Extended))
		{ "61927.dat",       "62271c01.dat", "62274c01.dat", "47157.dat", 170.00f,  0.0f }, // Moved to 61927-f1 (was Technic Power Functions Linear Actuator (Contracted))
		{ "92693C01-F1.dat", "92693c01.dat", "92696.dat",    "92695.dat", 120.00f, 30.0f }, // Technic Linear Actuator 4 x 1 x 1 (Contracted)
		{ "92693C01-F2.dat", "92693c01.dat", "92696.dat",    "92695.dat", 180.00f, 30.0f }, // Technic Linear Actuator 4 x 1 x 1 (Extended)
		{ "92693C02.dat",    "92693c01.dat", "92696.dat",    "92695.dat", 120.00f, 30.0f }, // Moved to 92693c01-f1 (was Technic Linear Actuator Small (Contracted))
		{ "92693C03.dat",    "92693c01.dat", "92696.dat",    "92695.dat", 180.00f, 30.0f }, // Moved to 92693c01-f2 (was Technic Linear Actuator Small (Extended))
	};

	for (const auto& ActuatorInfo: Actuators)
	{
		swPartData* PartData = Library->FindPartData(ActuatorInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationActuator(ActuatorInfo.BodyPart, ActuatorInfo.PistonPart,
					ActuatorInfo.AxlePart, ActuatorInfo.Length, ActuatorInfo.AxleOffset));
	}

	static const struct
	{
		char PartID[16];
		float Length;
		float EndOffset;
		char EndPart[16];
		char CenterPart[16];
	}
	UniversalJoints[] =
	{
		{ "61903.dat",   60.00f,  0.0f, "62520.dat", "62519.dat" }, // Technic Universal Joint 3L (Complete)
		{ "3712C01.dat", 60.00f, 30.0f, "3712.dat",  "3326.dat"  }, // Technic Universal Joint 4L with Bush Ends with Centre Type 2 (Complete)
		{ "3712C03.dat", 60.00f, 30.0f, "3712.dat",  "3326a.dat" }, // Technic Universal Joint 4L with Bush Ends with Centre Type 1 (Complete)
		{ "575C01.dat",  60.00f, 30.0f, "575.dat",   "3326a.dat" }  // Technic Universal Joint Type 1 (Complete)
	};

	for (const auto& JointInfo: UniversalJoints)
	{
		swPartData* PartData = Library->FindPartData(JointInfo.PartID, nullptr);

		if (PartData)
			PartData->SetSynthInfo(new swSynthesisInformationUniversalJoint(JointInfo.Length, JointInfo.EndOffset, JointInfo.EndPart, JointInfo.CenterPart));
	}

//	"758C01" // Hose Flexible  12L
}

swSynthesisInformation::swSynthesisInformation(float Length)
	: mLength(Length)
{
}

swSynthesisInformationCurved::swSynthesisInformationCurved(float Length, float DefaultScale, int NumSections, bool RigidEdges)
	: swSynthesisInformation(Length), mSectionCount(NumSections), mDefaultScale(DefaultScale), mRigidEdges(RigidEdges)
{
	mCurve = true;

	mStart.Transform = swMatrix44(swMatrix33(swVector3(0.0f, 0.0f, 1.0f), swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 1.0f, 0.0f)), swVector3(0.0f, 0.0f, 0.0f));
	mMiddle.Length = 0.0f;
	mEnd.Transform = swMatrix44(swMatrix33(swVector3(0.0f, 0.0f, 1.0f), swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 1.0f, 0.0f)), swVector3(0.0f, 0.0f, 0.0f));
}

swSynthesisInformationFlexibleHose::swSynthesisInformationFlexibleHose(float Length, int NumSections, const char* EdgePart2)
	: swSynthesisInformationCurved(Length, 12.f, NumSections, true),
	mEdgePart2(EdgePart2)
{
	mStart.Length = 5.0f;
	mMiddle.Length = 2.56f;
	mEnd.Length = 5.0f;
	mCenterLength = 4.56f;
}

swSynthesisInformationFlexSystemHose::swSynthesisInformationFlexSystemHose(float Length, int NumSections)
	: swSynthesisInformationCurved(Length, 12.f, NumSections, true)
{
	mStart.Transform = swMatrix44Identity();
	mEnd.Transform = swMatrix44Identity();
	mStart.Length = 1.0f;
	mMiddle.Length = 2.0f;
	mEnd.Length = 1.0f;
}

swSynthesisInformationPneumaticTube::swSynthesisInformationPneumaticTube(float Length, int NumSections, const char* EndPart)
	: swSynthesisInformationCurved(Length, 12.f, NumSections, true),
	mEndPart(EndPart)
{
	mStart.Length = 0.0f;
	mMiddle.Length = mLength / NumSections;
	mEnd.Length = 0.0f;
}

swSynthesisInformationRibbedHose::swSynthesisInformationRibbedHose(float Length, int NumSections)
	: swSynthesisInformationCurved(Length, 80.0f, NumSections, false)
{
	mStart.Length = 6.25f;
	mMiddle.Length = 6.25f;
	mEnd.Length = 6.25f;
}

swSynthesisInformationFlexibleAxle::swSynthesisInformationFlexibleAxle(float Length, int NumSections)
	: swSynthesisInformationCurved(Length, 12.0f, NumSections, true)
{
	mStart.Length = 30.0f;
	mMiddle.Length = 4.0f;
	mEnd.Length = 30.0f;
}

swSynthesisInformationBraidedString::swSynthesisInformationBraidedString(float Length, int NumSections)
	: swSynthesisInformationCurved(Length, 12.0f, NumSections, true)
{
	mStart.Transform = swMatrix44Identity();
	mEnd.Transform = swMatrix44Identity();
	mStart.Length = 8.0f;
	mMiddle.Length = 4.0f;
	mEnd.Length = 8.0f;
}

swSynthesisInformationStraight::swSynthesisInformationStraight(float Length)
	: swSynthesisInformation(Length)
{
	mUnidirectional = true;
}

swSynthesisInformationShockAbsorber::swSynthesisInformationShockAbsorber(const char* SpringPart)
	: swSynthesisInformationStraight(110.00f), mSpringPart(SpringPart)
{
}

swSynthesisInformationActuator::swSynthesisInformationActuator(const char* BodyPart, const char* PistonPart, const char* AxlePart, float Length, float AxleOffset)
	: swSynthesisInformationStraight(Length), mBodyPart(BodyPart), mPistonPart(PistonPart), mAxlePart(AxlePart), mAxleOffset(AxleOffset)
{
}

swSynthesisInformationUniversalJoint::swSynthesisInformationUniversalJoint(float Length, float EndOffset, const char* EndPart, const char* CenterPart)
	: swSynthesisInformation(Length), mEndOffset(EndOffset), mEndPart(EndPart), mCenterPart(CenterPart)
{
	mNondirectional = true;
}

void swSynthesisInformationCurved::GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	ControlPoints.resize(2);

	float HalfLength = mLength / 2.0f;
	float Scale = swMinimum(mDefaultScale, HalfLength);

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(-HalfLength, 0.0f, 0.0f));
	ControlPoints[1].Transform = swMatrix44Translation(swVector3( HalfLength, 0.0f, 0.0f));

	ControlPoints[0].Scale = Scale;
	ControlPoints[1].Scale = Scale;
}

void swSynthesisInformationCurved::VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	if (ControlPoints.size() < 2)
		GetDefaultControlPoints(ControlPoints);
}

void swSynthesisInformationFlexSystemHose::GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	swSynthesisInformationCurved::GetDefaultControlPoints(ControlPoints);

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, -mLength));
	ControlPoints[1].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, 0.0f));
}

void swSynthesisInformationStraight::VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	if (ControlPoints.size() < 2)
		GetDefaultControlPoints(ControlPoints);
	else
		ControlPoints.resize(2);
}

void swSynthesisInformationShockAbsorber::GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	ControlPoints.resize(2);

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, -mLength));
	ControlPoints[1].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, 0.0f));

	ControlPoints[0].Scale = 1.0f;
	ControlPoints[1].Scale = 1.0f;
}

void swSynthesisInformationActuator::GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	ControlPoints.resize(2);

	ControlPoints[0].Transform = swMatrix44(swMatrix33(swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, 1.0f, 0.0f)), swVector3(0.0f, 0.0f, 0.0f));
	ControlPoints[1].Transform = swMatrix44(swMatrix33(swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, 1.0f, 0.0f)), swVector3(0.0f, mLength, 0.0f));

	ControlPoints[0].Scale = 1.0f;
	ControlPoints[1].Scale = 1.0f;
}

void swSynthesisInformationUniversalJoint::GetDefaultControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	ControlPoints.resize(1);
	float HalfLength = mLength / 2;

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(0.0f, HalfLength, 0.0f));
	ControlPoints[0].Scale = 1.0f;
}

void swSynthesisInformationUniversalJoint::VerifyControlPoints(std::vector<swPartControlPoint>& ControlPoints) const
{
	if (ControlPoints.empty())
		GetDefaultControlPoints(ControlPoints);
	else
		ControlPoints.resize(1);
}

float swSynthesisInformationCurved::GetSectionTwist(const swMatrix44& StartTransform, const swMatrix44& EndTransform) const
{
	swVector3 StartTangent(StartTransform[1].x, StartTransform[1].y, StartTransform[1].z);
	swVector3 EndTangent(EndTransform[1].x, EndTransform[1].y, EndTransform[1].z);
	swVector3 StartUp(StartTransform[2].x, StartTransform[2].y, StartTransform[2].z);
	swVector3 EndUp(EndTransform[2].x, EndTransform[2].y, EndTransform[2].z);

	float TangentDot = swDot(StartTangent, EndTangent);
	float UpDot = swDot(StartUp, EndUp);

	if (TangentDot > 0.99f && UpDot > 0.99f)
		return 0.0f;

	if (fabs(TangentDot) > 0.99f)
	{
		return acosf(swClamp(swDot(EndUp, StartUp), -1.0f, 1.0f));
	}
	else if (TangentDot > -0.99f)
	{
		swVector3 Axis = swCross(StartTangent, EndTangent);
		float Angle = acosf(swClamp(TangentDot, -1.0f, 1.0f));

		swMatrix33 Rotation = swMatrix33FromAxisAngle(Axis, Angle);
		swVector3 AdjustedStartUp = swMultiply(StartUp, Rotation);
		return acosf(swClamp(swDot(EndUp, AdjustedStartUp), -1.0f, 1.0f));
	}

	swVector3 StartSide(StartTransform[0].x, StartTransform[0].y, StartTransform[0].z);
	swVector3 EndSide(EndTransform[0].x, EndTransform[0].y, EndTransform[0].z);

	float SideDot = swDot(StartSide, EndSide);

	if (fabs(SideDot) < 0.99f)
	{
		swVector3 Axis = swCross(StartSide, EndSide);
		float Angle = acosf(SideDot);

		swMatrix33 Rotation = swMatrix33FromAxisAngle(Axis, Angle);
		swVector3 AdjustedStartUp = swMultiply(StartUp, Rotation);
		return acosf(swClamp(swDot(EndUp, AdjustedStartUp), -1.0f, 1.0f));
	}

	return 0.0f;
}

// use to turn a section back into the control point rotation that leaves along it
swMatrix33 swSynthesisInformationCurved::GetSectionControlPointRotation(const swMatrix44& Section, const swMatrix33& Rotation) const
{
	Q_UNUSED(Rotation);

	const swMatrix33 LDrawRotation = swMultiply(swMultiply(swMatrix33Transpose(swMatrix33(mStart.Transform)), swMatrix33(Section)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f)));
	return swMatrix33(swMatrix44LDrawToStudWorks(swMatrix44(LDrawRotation, swVector3(0.0f, 0.0f, 0.0f))));
}

// use to get the path length between a section and the next one
float swSynthesisInformationCurved::GetSectionSpacing(size_t SectionCount) const
{
	float Spacing = mMiddle.Length;

	if (mCenterLength != 0.0f && SectionCount == mSectionCount / 2 + 1)
		Spacing = mCenterLength;

	if (SectionCount == mSectionCount + 1 && !mRigidEdges)
		Spacing += mEnd.Length;

	return Spacing;
}

// use to get the path length the sections take up when laid out along the path
float swSynthesisInformationCurved::GetNominalLength() const
{
	float NominalLength = mRigidEdges ? 0.0f : mStart.Length;

	for (size_t SectionCount = 2; SectionCount <= mSectionCount + 1; SectionCount++)
		NominalLength += GetSectionSpacing(SectionCount);

	return NominalLength;
}

// use to check the part reaches every control point when its path runs straight between them
bool swSynthesisInformationCurved::IsReachable(const std::vector<swPartControlPoint>& ControlPoints) const
{
	if (ControlPoints.size() < 2)
		return true;

	const std::vector<float> Scales(ControlPoints.size(), 0.0f);

	return GetPathLength(ControlPoints, Scales, 2, nullptr) <= GetNominalLength();
}

// use to get the path line from the first to the last control point and how much of it the part covers
void swSynthesisInformationCurved::GetPath(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swVector3>& Points, size_t& ReachedPointCount) const
{
	Points.clear();
	ReachedPointCount = 0;

	if (ControlPoints.size() < 2)
		return;

	const int NumPathPoints = 48;
	const float NominalLength = GetNominalLength();
	const std::vector<float> Scales = GetFittedScales(ControlPoints, NominalLength, nullptr);
	float PathLength = 0.0f;
	bool Reached = true;
	swVector3 PreviousPosition;

	auto AddPoint = [&Points](const swVector3& Position)
	{
		Points.emplace_back(Position[0], Position[2], -Position[1]);
	};

	Points.emplace_back(ControlPoints.front().Transform.GetTranslation());

	for (quint32 ControlPointIndex = 0; ControlPointIndex + 1 < ControlPoints.size(); ControlPointIndex++)
	{
		swVector3 SegmentControlPoints[4];
		swMatrix44 StartTransform, EndTransform;
		GetSegmentControlPoints(ControlPoints, Scales, ControlPointIndex, SegmentControlPoints, StartTransform, EndTransform);

		for (int PointIdx = ControlPointIndex == 0 ? 0 : 1; PointIdx < NumPathPoints; PointIdx++)
		{
			float t = (float)PointIdx / (float)(NumPathPoints - 1);
			float it = 1.0f - t;

			swVector3 Position = it * it * it * SegmentControlPoints[0] + it * it * 3.0f * t * SegmentControlPoints[1] + it * 3.0f * t * t * SegmentControlPoints[2] + t * t * t * SegmentControlPoints[3];

			if (ControlPointIndex != 0 || PointIdx != 0)
			{
				float StepLength = swLength(Position - PreviousPosition);

				// split the line where the part runs out
				if (Reached && PathLength + StepLength > NominalLength + 0.01f && StepLength > 0.0f)
				{
					AddPoint(PreviousPosition + (Position - PreviousPosition) * ((NominalLength - PathLength) / StepLength));
					ReachedPointCount = Points.size();
					Reached = false;
				}

				PathLength += StepLength;
			}

			AddPoint(Position);
			PreviousPosition = Position;
		}
	}

	Points.emplace_back(ControlPoints.back().Transform.GetTranslation());

	if (Reached)
		ReachedPointCount = Points.size();
}

// use to get how far before the end control point the path stops so the end fitting sits on it like the start one
float swSynthesisInformationCurved::GetEndInset() const
{
	return mLength - (mRigidEdges ? mStart.Length : 0.0f) - GetNominalLength();
}

// use to turn a middle control point along the way the path runs so it never folds back on itself
float swSynthesisInformationCurved::GetHandleDirection(const std::vector<swPartControlPoint>& ControlPoints, quint32 ControlPointIndex) const
{
	if (ControlPointIndex == 0 || ControlPointIndex + 1 >= ControlPoints.size())
		return 1.0f;

	const swMatrix44 Transform = swMatrix44StudWorksToLDraw(ControlPoints[ControlPointIndex].Transform);
	const swMatrix33 Rotation = swMultiply(swMultiply(swMatrix33(mStart.Transform), swMatrix33(Transform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f)));
	const swVector3 Tangent = swMultiply(swVector3(0.0f, 1.0f, 0.0f), Rotation);
	const swVector3 Chord = ControlPoints[ControlPointIndex + 1].Transform.GetTranslation() - ControlPoints[ControlPointIndex - 1].Transform.GetTranslation();
	const swVector3 LDrawChord(Chord[0], -Chord[2], Chord[1]);

	return swDot(Tangent, LDrawChord) < 0.0f ? -1.0f : 1.0f;
}

// use to get the bezier points of a path segment with the given handle lengths
void swSynthesisInformationCurved::GetSegmentControlPoints(const std::vector<swPartControlPoint>& ControlPoints, const std::vector<float>& Scales, quint32 ControlPointIndex, swVector3 SegmentControlPoints[4], swMatrix44& StartTransform, swMatrix44& EndTransform) const
{
	StartTransform = swMatrix44StudWorksToLDraw(ControlPoints[ControlPointIndex].Transform);
	EndTransform = swMatrix44StudWorksToLDraw(ControlPoints[ControlPointIndex + 1].Transform);
	StartTransform = swMatrix44(swMultiply(swMultiply(swMatrix33(mStart.Transform), swMatrix33(StartTransform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), StartTransform.GetTranslation());
	EndTransform = swMatrix44(swMultiply(swMultiply(swMatrix33(mEnd.Transform), swMatrix33(EndTransform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), EndTransform.GetTranslation());

	// rigid ends keep their fittings on the control points
	if (ControlPointIndex == 0 && mRigidEdges)
		StartTransform.SetTranslation(swMultiply31(swVector3(0.0f, mStart.Length, 0.0f), StartTransform));

	if (ControlPointIndex + 2 == ControlPoints.size())
		EndTransform.SetTranslation(swMultiply31(swVector3(0.0f, -GetEndInset(), 0.0f), EndTransform));

	SegmentControlPoints[0] = StartTransform.GetTranslation();
	SegmentControlPoints[1] = swMultiply31(swVector3(0.0f, Scales[ControlPointIndex] * GetHandleDirection(ControlPoints, ControlPointIndex), 0.0f), StartTransform);
	SegmentControlPoints[2] = swMultiply31(swVector3(0.0f, -Scales[ControlPointIndex + 1] * GetHandleDirection(ControlPoints, ControlPointIndex + 1), 0.0f), EndTransform);
	SegmentControlPoints[3] = EndTransform.GetTranslation();
}

// use to measure the whole path and how much it bends
float swSynthesisInformationCurved::GetPathLength(const std::vector<swPartControlPoint>& ControlPoints, const std::vector<float>& Scales, int CurvePointCount, float* BendEnergy) const
{
	float PathLength = 0.0f;
	float Energy = 0.0f;
	swVector3 PreviousDirection(0.0f, 0.0f, 0.0f);

	for (quint32 ControlPointIndex = 0; ControlPointIndex + 1 < ControlPoints.size(); ControlPointIndex++)
	{
		swVector3 SegmentControlPoints[4];
		swMatrix44 StartTransform, EndTransform;
		GetSegmentControlPoints(ControlPoints, Scales, ControlPointIndex, SegmentControlPoints, StartTransform, EndTransform);

		swVector3 PreviousPosition = SegmentControlPoints[0];

		for (int PointIdx = 1; PointIdx < CurvePointCount; PointIdx++)
		{
			float t = (float)PointIdx / (float)(CurvePointCount - 1);
			float it = 1.0f - t;

			swVector3 Position = it * it * it * SegmentControlPoints[0] + it * it * 3.0f * t * SegmentControlPoints[1] + it * 3.0f * t * t * SegmentControlPoints[2] + t * t * t * SegmentControlPoints[3];
			swVector3 Step = Position - PreviousPosition;
			float StepLength = swLength(Step);

			if (StepLength > 1e-4f)
			{
				swVector3 Direction = Step / StepLength;

				// turning angle squared over the step length sums up the squared curvature
				if (BendEnergy && swLength(PreviousDirection) > 0.0f)
				{
					float Angle = acosf(swClamp(swDot(Direction, PreviousDirection), -1.0f, 1.0f));
					Energy += Angle * Angle / StepLength;
				}

				PreviousDirection = Direction;
			}

			PathLength += StepLength;
			PreviousPosition = Position;
		}
	}

	if (BendEnergy)
		*BendEnergy = Energy;

	return PathLength;
}

// use to scale all handles together until the path is as long as the part
float swSynthesisInformationCurved::FitPathLength(const std::vector<swPartControlPoint>& ControlPoints, std::vector<float>& Scales, float NominalLength, float* BendEnergy) const
{
	const int NumFitPoints = 128;
	std::vector<float> FitScales(Scales.size(), 0.0f);

	// the ends are too far apart for the part, keep the handles
	if (GetPathLength(ControlPoints, FitScales, NumFitPoints, nullptr) >= NominalLength)
		return GetPathLength(ControlPoints, Scales, NumFitPoints, BendEnergy);

	float LowFactor = 0.0f;
	float HighFactor = 1.0f;

	auto GetScaledLength = [&](float Factor)
	{
		for (size_t ScaleIndex = 0; ScaleIndex < Scales.size(); ScaleIndex++)
			FitScales[ScaleIndex] = Scales[ScaleIndex] * Factor;

		return GetPathLength(ControlPoints, FitScales, NumFitPoints, nullptr);
	};

	while (GetScaledLength(HighFactor) < NominalLength && HighFactor < 65536.0f)
	{
		LowFactor = HighFactor;
		HighFactor *= 2.0f;
	}

	for (int Iteration = 0; Iteration < 24; Iteration++)
	{
		float MiddleFactor = (LowFactor + HighFactor) / 2.0f;

		if (GetScaledLength(MiddleFactor) < NominalLength)
			LowFactor = MiddleFactor;
		else
			HighFactor = MiddleFactor;
	}

	const float Factor = (LowFactor + HighFactor) / 2.0f;

	for (float& Scale : Scales)
		Scale *= Factor;

	return GetPathLength(ControlPoints, Scales, NumFitPoints, BendEnergy);
}

// use to find the handle lengths that keep the part length and bend it the least like a real hose
std::vector<float> swSynthesisInformationCurved::GetFittedScales(const std::vector<swPartControlPoint>& ControlPoints, float NominalLength, const std::atomic_bool* Cancelled) const
{
	std::vector<float> Scales;

	for (const swPartControlPoint& ControlPoint : ControlPoints)
		Scales.emplace_back(swMaximum(ControlPoint.Scale, 1.0f));

	if (ControlPoints.size() < 2 || NominalLength <= 0.0f)
		return Scales;

	// a part too short for its path follows handles a third of the shorter chord beside each point as far as it reaches
	std::vector<float> ChordScales(Scales.size(), 0.0f);

	if (GetPathLength(ControlPoints, ChordScales, 2, nullptr) >= NominalLength)
	{
		ChordScales.assign(Scales.size(), FLT_MAX);

		for (quint32 ControlPointIndex = 0; ControlPointIndex + 1 < ControlPoints.size(); ControlPointIndex++)
		{
			swVector3 SegmentControlPoints[4];
			swMatrix44 StartTransform, EndTransform;
			GetSegmentControlPoints(ControlPoints, Scales, ControlPointIndex, SegmentControlPoints, StartTransform, EndTransform);

			const float Chord = swLength(SegmentControlPoints[3] - SegmentControlPoints[0]) / 3.0f;
			ChordScales[ControlPointIndex] = swMinimum(ChordScales[ControlPointIndex], Chord);
			ChordScales[ControlPointIndex + 1] = swMinimum(ChordScales[ControlPointIndex + 1], Chord);
		}

		return ChordScales;
	}

	float BestEnergy = 0.0f;
	FitPathLength(ControlPoints, Scales, NominalLength, &BestEnergy);

	// walk each handle up and down while the bending drops, with smaller steps each round
	for (float StepFactor = 2.0f; StepFactor > 1.01f; StepFactor = sqrtf(StepFactor))
	{
		bool Improved = true;

		for (int Round = 0; Round < 16 && Improved; Round++)
		{
			Improved = false;

			for (size_t ScaleIndex = 0; ScaleIndex < Scales.size(); ScaleIndex++)
			{
				if (Cancelled && Cancelled->load(std::memory_order_relaxed))
					return Scales;

				for (float Step : { StepFactor, 1.0f / StepFactor })
				{
					std::vector<float> TryScales = Scales;
					TryScales[ScaleIndex] *= Step;

					float Energy = 0.0f;
					FitPathLength(ControlPoints, TryScales, NominalLength, &Energy);

					if (Energy < BestEnergy * 0.999f)
					{
						BestEnergy = Energy;
						Scales = TryScales;
						Improved = true;
						break;
					}
				}
			}
		}
	}

	return Scales;
}

void swSynthesisInformationCurved::CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
{
	if (ControlPoints.empty())
		return;

	const int NumCurvePoints = 4096;

	const float NominalLength = GetNominalLength();
	const std::vector<float> Scales = GetFittedScales(ControlPoints, NominalLength, Cancelled);
	float SectionStretch = 1.0f;

	// spread the sections over the actual path so the last one lands on the end point, a real part never stretches
	if (ControlPoints.size() > 1 && NominalLength > 0.0f)
		SectionStretch = swMinimum(GetPathLength(ControlPoints, Scales, NumCurvePoints, nullptr) / NominalLength, 1.0f);

	float SectionLength = 0.0f;

	for (quint32 ControlPointIndex = 0; ControlPointIndex < ControlPoints.size() - 1 && Sections.size() < mSectionCount + 2; ControlPointIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swVector3 SegmentControlPoints[4];
		swMatrix44 StartTransform, EndTransform;
		GetSegmentControlPoints(ControlPoints, Scales, ControlPointIndex, SegmentControlPoints, StartTransform, EndTransform);

		if (ControlPointIndex == 0)
		{
			SectionLength = mRigidEdges ? 0.0f : mStart.Length * SectionStretch;
			Sections.emplace_back(StartTransform);
		}

		std::vector<swVector3> CurvePoints;
		CurvePoints.reserve(NumCurvePoints);

		for (int PointIdx = 0; PointIdx < NumCurvePoints; PointIdx++)
		{
			if ((PointIdx & 63) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
				return;

			float t = (float)PointIdx / (float)(NumCurvePoints - 1);
			float it = 1.0f - t;

			swVector3 Position = it * it * it * SegmentControlPoints[0] + it * it * 3.0f * t * SegmentControlPoints[1] + it * 3.0 * t * t * SegmentControlPoints[2] + t * t * t * SegmentControlPoints[3];
			CurvePoints.emplace_back(Position);
		}

		float CurrentSegmentLength = 0.0f;
		float TotalSegmentLength = 0.0f;

		for (size_t PointIdx = 0; PointIdx < CurvePoints.size() - 1; PointIdx++)
		{
			if ((PointIdx & 63) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
				return;

			TotalSegmentLength += swLength(CurvePoints[PointIdx] - CurvePoints[PointIdx + 1]);
		}

		swVector3 StartUp = swMultiply30(swVector3(1.0f, 0.0f, 0.0f), StartTransform);
		float Twist = GetSectionTwist(StartTransform, EndTransform);
		size_t CurrentPointIndex = 0;

		while (CurrentPointIndex < CurvePoints.size() - 1)
		{
			if ((CurrentPointIndex & 63) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
				return;

			float Length = swLength(CurvePoints[CurrentPointIndex + 1] - CurvePoints[CurrentPointIndex]);
			CurrentSegmentLength += Length;
			SectionLength -= Length;
			CurrentPointIndex++;

			if (SectionLength > 0.0f)
				continue;

			float t = (float)CurrentPointIndex / (float)(NumCurvePoints - 1);
			float it = 1.0f - t;

			swVector3 Tangent = swNormalize(-3.0f * it * it * SegmentControlPoints[0] + (3.0f * it * it - 6.0f * t * it) * SegmentControlPoints[1] + (-3.0f * t * t + 6.0f * t * it) * SegmentControlPoints[2] + 3.0f * t * t * SegmentControlPoints[3]);
			swVector3 Up;

			if (Twist)
			{
				Up = swMultiply(StartUp, swMatrix33FromAxisAngle(Tangent, Twist * (CurrentSegmentLength / TotalSegmentLength)));
				CurrentSegmentLength = 0.0f;
			}
			else
				Up = StartUp;

			swVector3 Side = swNormalize(swCross(Tangent, Up));
			Up = swNormalize(swCross(Side, Tangent));
			StartUp = Up;

			Sections.emplace_back(swMatrix44(swMatrix33(Up, Tangent, Side), CurvePoints[CurrentPointIndex]));

			if (SectionCallback)
				SectionCallback(CurvePoints[CurrentPointIndex], ControlPointIndex, t);

			if (Sections.size() == mSectionCount + 2)
				break;

			SectionLength += GetSectionSpacing(Sections.size()) * SectionStretch;
		}
	}

	while (Sections.size() < mSectionCount + 2)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		// rounding can leave the last section a hair short of the end point
		swMatrix44 EndTransform = swMatrix44StudWorksToLDraw(ControlPoints.back().Transform);
		EndTransform = swMatrix44(swMultiply(swMultiply(swMatrix33(mEnd.Transform), swMatrix33(EndTransform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), EndTransform.GetTranslation());
		swVector3 Position = swMultiply31(swVector3(0.0f, SectionLength - (ControlPoints.size() > 1 ? GetEndInset() : 0.0f), 0.0f), EndTransform);
		EndTransform.SetTranslation(Position);
		Sections.emplace_back(EndTransform);

		if (SectionCallback)
			SectionCallback(Position, static_cast<quint32>(ControlPoints.size()) - 1, 1.0f);

		SectionLength += GetSectionSpacing(Sections.size()) * SectionStretch;
	}
}

void swSynthesisInformationBraidedString::CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
{
	if (ControlPoints.empty())
		return;

	float SectionLength = 0.0f;

	for (quint32 ControlPointIndex = 0; ControlPointIndex < ControlPoints.size() - 1 && Sections.size() < mSectionCount + 2; ControlPointIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swVector3 SegmentControlPoints[4];

		swMatrix44 StartTransform = swMatrix44StudWorksToLDraw(ControlPoints[ControlPointIndex].Transform);
		swMatrix44 EndTransform = swMatrix44StudWorksToLDraw(ControlPoints[ControlPointIndex + 1].Transform);
		StartTransform = swMatrix44(swMultiply(swMultiply(swMatrix33(mStart.Transform), swMatrix33(StartTransform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), StartTransform.GetTranslation());

		if (ControlPointIndex == 0)
		{
			if (mRigidEdges)
			{
				StartTransform.SetTranslation(swMultiply30(swVector3(mStart.Length, 0.0f, 0.0f), StartTransform) + StartTransform.GetTranslation());
				SectionLength = 0.0f;
			}
			else
				SectionLength = mStart.Length;

			Sections.emplace_back(StartTransform);
		}

		EndTransform = swMatrix44(swMultiply(swMultiply(swMatrix33(mEnd.Transform), swMatrix33(EndTransform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), EndTransform.GetTranslation());

		SegmentControlPoints[0] = StartTransform.GetTranslation();
		SegmentControlPoints[1] = swMultiply31(swVector3(ControlPoints[ControlPointIndex].Scale, 0.0f, 0.0f), StartTransform);
		SegmentControlPoints[2] = swMultiply31(swVector3(-ControlPoints[ControlPointIndex + 1].Scale, 0.0f, 0.0f), EndTransform);
		SegmentControlPoints[3] = EndTransform.GetTranslation();

		const int NumCurvePoints = 4096;
		std::vector<swVector3> CurvePoints;
		CurvePoints.reserve(NumCurvePoints);

		for (int PointIdx = 0; PointIdx < NumCurvePoints; PointIdx++)
		{
			if ((PointIdx & 63) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
				return;

			float t = (float)PointIdx / (float)(NumCurvePoints - 1);
			float it = 1.0f - t;

			swVector3 Position = it * it * it * SegmentControlPoints[0] + it * it * 3.0f * t * SegmentControlPoints[1] + it * 3.0 * t * t * SegmentControlPoints[2] + t * t * t * SegmentControlPoints[3];
			CurvePoints.emplace_back(Position);
		}

		float CurrentSegmentLength = 0.0f;
		float TotalSegmentLength = 0.0f;

		for (size_t PointIdx = 0; PointIdx < CurvePoints.size() - 1; PointIdx++)
		{
			if ((PointIdx & 63) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
				return;

			TotalSegmentLength += swLength(CurvePoints[PointIdx] - CurvePoints[PointIdx + 1]);
		}

		swVector3 StartUp = swMultiply30(swVector3(0.0f, 1.0f, 0.0f), StartTransform);
		float Twist = GetSectionTwist(StartTransform, EndTransform);
		size_t CurrentPointIndex = 0;

		while (CurrentPointIndex < CurvePoints.size() - 1)
		{
			if ((CurrentPointIndex & 63) == 0 && Cancelled && Cancelled->load(std::memory_order_relaxed))
				return;

			float Length = swLength(CurvePoints[CurrentPointIndex + 1] - CurvePoints[CurrentPointIndex]);
			CurrentSegmentLength += Length;
			SectionLength -= Length;
			CurrentPointIndex++;

			if (SectionLength > 0.0f)
				continue;

			float t = (float)CurrentPointIndex / (float)(NumCurvePoints - 1);
			float it = 1.0f - t;

			swVector3 Tangent = swNormalize(-3.0f * it * it * SegmentControlPoints[0] + (3.0f * it * it - 6.0f * t * it) * SegmentControlPoints[1] + (-3.0f * t * t + 6.0f * t * it) * SegmentControlPoints[2] + 3.0f * t * t * SegmentControlPoints[3]);
			swVector3 Up;

			if (Twist)
			{
				Up = swMultiply(StartUp, swMatrix33FromAxisAngle(Tangent, Twist * (CurrentSegmentLength / TotalSegmentLength)));
				CurrentSegmentLength = 0.0f;
			}
			else
				Up = StartUp;

			swVector3 Side = swNormalize(swCross(Tangent, Up));
			Up = swNormalize(swCross(Side, Tangent));
			StartUp = Up;

			Sections.emplace_back(swMatrix44(swMatrix33(Tangent, Up, -Side), CurvePoints[CurrentPointIndex]));

			if (SectionCallback)
				SectionCallback(CurvePoints[CurrentPointIndex], ControlPointIndex, t);

			if (Sections.size() == mSectionCount + 2)
				break;

			if (mCenterLength != 0.0f && (Sections.size() == mSectionCount / 2 + 1))
				SectionLength += mCenterLength;
			else
				SectionLength += mMiddle.Length;

			if (Sections.size() == mSectionCount + 1 && !mRigidEdges)
				SectionLength += mEnd.Length;
		}
	}

	while (Sections.size() < mSectionCount + 2)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swMatrix44 EndTransform = swMatrix44StudWorksToLDraw(ControlPoints.back().Transform);
		EndTransform = swMatrix44(swMultiply(swMultiply(swMatrix33(mEnd.Transform), swMatrix33(EndTransform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), EndTransform.GetTranslation());
		swVector3 Position = swMultiply31(swVector3(SectionLength, 0.0f, 0.0f), EndTransform);
		EndTransform.SetTranslation(Position);
		Sections.emplace_back(EndTransform);

		if (SectionCallback)
			SectionCallback(Position, static_cast<quint32>(ControlPoints.size()) - 1, 1.0f);

		if (mCenterLength != 0.0f && (Sections.size() == mSectionCount / 2 + 1))
			SectionLength += mCenterLength;
		else
			SectionLength += mMiddle.Length;

		if (Sections.size() == mSectionCount + 1 && !mRigidEdges)
			SectionLength += mEnd.Length;
	}
}

void swSynthesisInformationStraight::CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
{
	for (quint32 ControlPointIndex = 0; ControlPointIndex < ControlPoints.size(); ControlPointIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swMatrix44 Transform = swMatrix44StudWorksToLDraw(ControlPoints[ControlPointIndex].Transform);
		Sections.emplace_back(Transform);

		if (SectionCallback)
			SectionCallback(Transform.GetTranslation(), ControlPointIndex, 1.0f);
	}
}

void swSynthesisInformationUniversalJoint::CalculateSections(const std::vector<swPartControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
{
	for (quint32 ControlPointIndex = 0; ControlPointIndex < ControlPoints.size(); ControlPointIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swMatrix44 Transform = swMatrix44StudWorksToLDraw(ControlPoints[ControlPointIndex].Transform);
		Sections.emplace_back(Transform);

		if (SectionCallback)
			SectionCallback(Transform.GetTranslation(), ControlPointIndex, 1.0f);
	}
}

void swSynthesisInformationFlexibleHose::AddParts(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
{
	if (Sections.empty())
		return;

	char Line[256];
	const int NumEdgeParts = 2;

	const swMatrix33 EdgeTransforms[2] =
	{
		swMatrix33(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, -1.0f, 0.0f), swVector3(1.0f, 0.0f, 0.0f)),
		swMatrix33(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f,  1.0f, 0.0f), swVector3(1.0f, 0.0f, 0.0f))
	};

	const char* EdgeParts[2] =
	{
		"755.dat", mEdgePart2
	};

	for (int PartIdx = 0; PartIdx < NumEdgeParts; PartIdx++)
	{
		const int SectionIndex = 0;
		swMatrix33 Transform(swMultiply(swMultiply(EdgeTransforms[PartIdx], swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, -5.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgeParts[PartIdx]);

		File.WriteBuffer(Line, strlen(Line));
	}

	for (size_t SectionIndex = 1; SectionIndex < Sections.size() - 1; SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swMatrix33 Transform = swMatrix33(Sections[SectionIndex]);
		swVector3 Offset = Sections[SectionIndex].GetTranslation();

		if (SectionIndex < Sections.size() / 2)
			Transform = swMultiply(swMatrix33(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, -1.0f, 0.0f), swVector3(-1.0f, 0.0f, 0.0f)), Transform);
		else if (SectionIndex != Sections.size() / 2)
			Transform = swMultiply(swMatrix33(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f,  1.0f, 0.0f), swVector3( 1.0f, 0.0f, 0.0f)), Transform);
		else
			Transform = swMultiply(swMatrix33(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f,  1.0f, 0.0f), swVector3( 1.0f, 0.0f, 0.0f)), Transform);

		const char* Part = SectionIndex != Sections.size() / 2 ? "754.dat" : "756.dat";

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], Part);

		File.WriteBuffer(Line, strlen(Line));
	}

	for (int PartIdx = 0; PartIdx < NumEdgeParts; PartIdx++)
	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 Transform(swMultiply(EdgeTransforms[PartIdx], swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 5.0f - 2.56f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgeParts[PartIdx]);

		File.WriteBuffer(Line, strlen(Line));
	}
}

void swSynthesisInformationCurved::AddTubeParts(swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, float Radius, bool IsInner, const std::atomic_bool* Cancelled)
{
	// a unit circle
	static const swVector3 Vertices[16] =
	{
		{  1.000000f, 0.0f,  0.000000f },
		{  0.923880f, 0.0f,  0.382683f },
		{  0.707107f, 0.0f,  0.707107f },
		{  0.382683f, 0.0f,  0.923880f },
		{  0.000000f, 0.0f,  1.000000f },
		{ -0.382683f, 0.0f,  0.923880f },
		{ -0.707107f, 0.0f,  0.707107f },
		{ -0.923880f, 0.0f,  0.382683f },
		{ -1.000000f, 0.0f,  0.000000f },
		{ -0.923879f, 0.0f, -0.382684f },
		{ -0.707107f, 0.0f, -0.707107f },
		{ -0.382683f, 0.0f, -0.923880f },
		{  0.000000f, 0.0f, -1.000000f },
		{  0.382684f, 0.0f, -0.923879f },
		{  0.707107f, 0.0f, -0.707107f },
		{  0.923880f, 0.0f, -0.382683f },
	};
	const int NumSectionVertices = SW_ARRAY_COUNT(Vertices);

	int BaseVertex;
	swMeshLoaderVertex* VertexBuffer;
	quint32* IndexBuffer;
	MeshData.AddVertices(SW_MESH_DATA_SHARED, NumSectionVertices * (Sections.size() - 1), &BaseVertex, &VertexBuffer);

	float NormalDirection = IsInner ? -1.0f : 1.0f;

	for (size_t SectionIndex = 1; SectionIndex < Sections.size(); SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		for (int VertexIdx = 0; VertexIdx < NumSectionVertices; VertexIdx++)
		{
			VertexBuffer->Position = swMultiply31(Radius * Vertices[VertexIdx], Sections[SectionIndex]);
			VertexBuffer->Normal = swMultiply30(NormalDirection * Vertices[VertexIdx], Sections[SectionIndex]);
			VertexBuffer->NormalWeight = 4.0f;
			VertexBuffer++;
		}
	}

	MeshData.AddIndices(SW_MESH_DATA_SHARED, SW_MESH_TRIANGLES, 16, 6 * NumSectionVertices * (Sections.size() - 2), &IndexBuffer);

	int Offset1, Offset2;
	if (IsInner)
	{
		Offset1 = NumSectionVertices - 1;	// -1 mod NumVertices
		Offset2 = 0;
	}
	else
	{
		Offset1 = 0;
		Offset2 = 1;
	}

	for (size_t SectionIndex = 1; SectionIndex < Sections.size() - 1; SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		for (int VertexIdx = 0; VertexIdx < NumSectionVertices; VertexIdx++)
		{
			int Vertex1 = BaseVertex + (VertexIdx + Offset1) % NumSectionVertices;
			int Vertex2 = BaseVertex + (VertexIdx + Offset2) % NumSectionVertices;

			*IndexBuffer++ = Vertex1;
			*IndexBuffer++ = Vertex2;
			*IndexBuffer++ = Vertex1 + NumSectionVertices;
			*IndexBuffer++ = Vertex2;
			*IndexBuffer++ = Vertex2 + NumSectionVertices;
			*IndexBuffer++ = Vertex1 + NumSectionVertices;
		}
		BaseVertex += NumSectionVertices;
	}
}

void swSynthesisInformationFlexSystemHose::AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
{
	if (Sections.empty())
		return;

	char Line[256];

	{
		const size_t SectionIndex = 0;
		swMatrix33 Transform(swMultiply(swMatrix33Scale(swVector3(-1.0f, 1.0f, 1.0f)), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, -1.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f u9053.dat\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2]);

		File.WriteBuffer(Line, strlen(Line));
	}

	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 Transform(swMultiply(swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f)), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 1.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f u9053.dat\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2]);

		File.WriteBuffer(Line, strlen(Line));
	}

	AddTubeParts(MeshData, Sections, 4.0f, false, Cancelled);
	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		return;

	AddTubeParts(MeshData, Sections, 2.0f, true, Cancelled);
}

void swSynthesisInformationPneumaticTube::AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
{
	if (Sections.empty())
		return;

	char Line[256];

	{
		const size_t SectionIndex = 0;
		swMatrix33 EdgeTransform(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, -1.0f, 0.0f), swVector3(1.0f, 0.0f, 0.0f));
		swMatrix33 Transform(swMultiply(swMultiply(EdgeTransform, swMatrix33Scale(swVector3(1.0f, 1.0f, 1.0f))), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 0.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPart);

		File.WriteBuffer(Line, strlen(Line));
	}

	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 EdgeTransform(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, -1.0f, 0.0f), swVector3(1.0f, 0.0f, 0.0f));
		swMatrix33 Transform(swMultiply(swMultiply(EdgeTransform, swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 0.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPart);

		File.WriteBuffer(Line, strlen(Line));
	}

	AddTubeParts(MeshData, Sections, 5.0f, false, Cancelled);
	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		return;

	AddTubeParts(MeshData, Sections, 3.0f, true, Cancelled);
}

void swSynthesisInformationRibbedHose::AddParts(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
{
	if (Sections.empty())
		return;

	char Line[256];

	{
		const int SectionIndex = 0;
		swMatrix33 Transform(swMultiply(swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f)), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = Sections[SectionIndex].GetTranslation();

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f 79.dat\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2]);

		File.WriteBuffer(Line, strlen(Line));
	}

	for (size_t SectionIndex = 1; SectionIndex < Sections.size() - 1; SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		const swMatrix44& Transform = Sections[SectionIndex];

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f 80.dat\n", Transform[3][0], Transform[3][1], Transform[3][2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2]);

		File.WriteBuffer(Line, strlen(Line));
	}

	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 Transform(Sections[SectionIndex]);
		swVector3 Offset = swMultiply31(swVector3(0.0f, -6.25f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f 79.dat\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2]);

		File.WriteBuffer(Line, strlen(Line));
	}
}

void swSynthesisInformationFlexibleAxle::AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
{
	if (Sections.empty())
		return;

	char Line[256];
	const int NumEdgeParts = 6;

	swMatrix33 EdgeTransforms[6] =
	{
		swMatrix33(swVector3(-1.0f, 0.0f, 0.0f), swVector3(0.0f, -5.0f, 0.0f), swVector3(0.0f, 0.0f,  1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
	};

	const char* EdgeParts[6] =
	{
		"stud3a.dat", "s/faxle1.dat", "s/faxle2.dat", "s/faxle3.dat", "s/faxle4.dat", "s/faxle5.dat"
	};

	for (int PartIdx = 0; PartIdx < NumEdgeParts; PartIdx++)
	{
		const int SectionIndex = 0;
		swMatrix33 Transform(swMultiply(swMultiply(EdgeTransforms[PartIdx], swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, -4.0f * (5 - PartIdx), 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgeParts[PartIdx]);

		File.WriteBuffer(Line, strlen(Line));
	}

	for (int PartIdx = 0; PartIdx < NumEdgeParts; PartIdx++)
	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 Transform(swMultiply(EdgeTransforms[PartIdx], swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 4.0f * (5 - PartIdx), 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgeParts[PartIdx]);

		File.WriteBuffer(Line, strlen(Line));
	}

	const swMeshLoaderVertex SectionVertices[28] =
	{
		{ swVector3(-6.000f, 0.0f,  0.000f), swVector3(-1.000f, 0.0f,  0.000f), 2.0f },
		{ swVector3(-5.602f, 0.0f,  2.000f), swVector3(-0.942f, 0.0f,  0.336f), 4.0f },
		{ swVector3(-5.602f, 0.0f,  2.000f), swVector3( 0.000f, 0.0f,  1.000f), 4.0f },
		{ swVector3(-2.000f, 0.0f,  2.000f), swVector3( 0.000f, 0.0f,  1.000f), 4.0f },
		{ swVector3(-2.000f, 0.0f,  2.000f), swVector3(-1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3(-2.000f, 0.0f,  5.602f), swVector3(-1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3(-2.000f, 0.0f,  5.602f), swVector3(-0.336f, 0.0f,  0.942f), 4.0f },
		{ swVector3( 0.000f, 0.0f,  6.000f), swVector3( 0.000f, 0.0f,  1.000f), 2.0f },
		{ swVector3( 2.000f, 0.0f,  5.602f), swVector3( 0.336f, 0.0f,  0.942f), 4.0f },
		{ swVector3( 2.000f, 0.0f,  5.602f), swVector3( 1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3( 2.000f, 0.0f,  2.000f), swVector3( 1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3( 2.000f, 0.0f,  2.000f), swVector3( 0.000f, 0.0f,  1.000f), 4.0f },
		{ swVector3( 5.602f, 0.0f,  2.000f), swVector3( 0.000f, 0.0f,  1.000f), 4.0f },
		{ swVector3( 5.602f, 0.0f,  2.000f), swVector3( 0.942f, 0.0f,  0.336f), 4.0f },
		{ swVector3( 6.000f, 0.0f,  0.000f), swVector3( 1.000f, 0.0f,  0.000f), 2.0f },
		{ swVector3( 5.602f, 0.0f, -2.000f), swVector3( 0.942f, 0.0f, -0.336f), 4.0f },
		{ swVector3( 5.602f, 0.0f, -2.000f), swVector3( 0.000f, 0.0f, -1.000f), 4.0f },
		{ swVector3( 2.000f, 0.0f, -2.000f), swVector3( 0.000f, 0.0f, -1.000f), 4.0f },
		{ swVector3( 2.000f, 0.0f, -2.000f), swVector3( 1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3( 2.000f, 0.0f, -5.602f), swVector3( 1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3( 2.000f, 0.0f, -5.602f), swVector3( 0.336f, 0.0f, -0.942f), 4.0f },
		{ swVector3( 0.000f, 0.0f, -6.000f), swVector3( 0.000f, 0.0f, -1.000f), 2.0f },
		{ swVector3(-2.000f, 0.0f, -5.602f), swVector3(-0.336f, 0.0f, -0.942f), 4.0f },
		{ swVector3(-2.000f, 0.0f, -5.602f), swVector3(-1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3(-2.000f, 0.0f, -2.000f), swVector3(-1.000f, 0.0f,  0.000f), 4.0f },
		{ swVector3(-2.000f, 0.0f, -2.000f), swVector3( 0.000f, 0.0f, -1.000f), 4.0f },
		{ swVector3(-5.602f, 0.0f, -2.000f), swVector3( 0.000f, 0.0f, -1.000f), 4.0f },
		{ swVector3(-5.602f, 0.0f, -2.000f), swVector3(-0.942f, 0.0f, -0.336f), 4.0f }
	};

	const int NumSectionVertices = SW_ARRAY_COUNT(SectionVertices);

	int BaseVertex;
	swMeshLoaderVertex* VertexBuffer;
	quint32* IndexBuffer;
	MeshData.AddVertices(SW_MESH_DATA_SHARED, NumSectionVertices * (Sections.size() - 1), &BaseVertex, &VertexBuffer);
	MeshData.AddIndices(SW_MESH_DATA_SHARED, SW_MESH_LINES, 24, 2 * 12 * (Sections.size() - 2), &IndexBuffer);

	for (size_t SectionIndex = 1; SectionIndex < Sections.size(); SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		for (int VertexIdx = 0; VertexIdx < NumSectionVertices; VertexIdx++)
		{
			VertexBuffer->Position = swMultiply31(SectionVertices[VertexIdx].Position, Sections[SectionIndex]);
			VertexBuffer->Normal = swMultiply30(SectionVertices[VertexIdx].Normal, Sections[SectionIndex]);
			VertexBuffer->NormalWeight = SectionVertices[VertexIdx].NormalWeight;
			VertexBuffer++;
		}
	}

	int BaseLinesVertex = BaseVertex;

	for (size_t SectionIndex = 1; SectionIndex < Sections.size() - 1; SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		const int Indices[] = { 1, 3, 5, 8, 10, 12, 15, 17, 19, 22, 24, 26 };

		for (int VertexIdx = 0; VertexIdx < 12; VertexIdx++)
		{
			*IndexBuffer++ = BaseLinesVertex + Indices[VertexIdx];
			*IndexBuffer++ = BaseLinesVertex + Indices[VertexIdx] + NumSectionVertices;
		}

		BaseLinesVertex += NumSectionVertices;
	}

	MeshData.AddIndices(SW_MESH_DATA_SHARED, SW_MESH_TRIANGLES, 16, 6 * NumSectionVertices * (Sections.size() - 2), &IndexBuffer);

	for (size_t SectionIndex = 1; SectionIndex < Sections.size() - 1; SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		for (int VertexIdx = 0; VertexIdx < NumSectionVertices; VertexIdx++)
		{
			int Vertex1 = BaseVertex + VertexIdx;
			int Vertex2 = BaseVertex + (VertexIdx + 1) % NumSectionVertices;

			*IndexBuffer++ = Vertex1;
			*IndexBuffer++ = Vertex2;
			*IndexBuffer++ = Vertex1 + NumSectionVertices;
			*IndexBuffer++ = Vertex2;
			*IndexBuffer++ = Vertex2 + NumSectionVertices;
			*IndexBuffer++ = Vertex1 + NumSectionVertices;
		}
		BaseVertex += NumSectionVertices;
	}
}

void swSynthesisInformationBraidedString::AddParts(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& SectionsIn, const std::atomic_bool* Cancelled) const
{
	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		return;

	std::vector<swMatrix44> Sections;
	Sections.resize(SectionsIn.size());

	for (size_t SectionIndex = 0; SectionIndex < Sections.size(); SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swMatrix33 Transform(swMultiply(swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f)), swMatrix33(SectionsIn[SectionIndex])));
		swVector3 Offset = SectionsIn[SectionIndex].GetTranslation();
		Sections[SectionIndex] = swMatrix44(Transform, Offset);
	}

	char Line[256];

	{
		const int SectionIndex = 0;
		swMatrix33 Transform(Sections[SectionIndex]);
		swVector3 Offset = swMultiply31(swVector3(-8.0f, 0.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f 572A.dat\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2]);

		File.WriteBuffer(Line, strlen(Line));
	}

	const size_t NumSegments = 16;
	const int NumBraids = 4;
	const float PositionTable[16] =
	{
		-1.5f, -1.386f, -1.061f, -0.574f, 0.0f, 0.574f, 1.061f, 1.386f, 1.5f, 1.386f, 1.061f, 0.574f, 0.0f, -0.574f, -1.061f, -1.386f
	};

	int BaseVertex;
	swMeshLoaderVertex* VertexBuffer;
	quint32* IndexBuffer;
	MeshData.AddVertices(SW_MESH_DATA_SHARED, NumBraids * ((Sections.size() - 2) * NumSegments + 1), &BaseVertex, &VertexBuffer);
	MeshData.AddIndices(SW_MESH_DATA_SHARED, SW_MESH_LINES, 24, NumBraids * (Sections.size() - 2) * NumSegments * 2, &IndexBuffer);

	for (int BraidIdx = 0; BraidIdx < NumBraids; BraidIdx++)
	{
		int BaseX = (BraidIdx == 0 || BraidIdx == 2) ? 0 : 8;
		int BaseY = (BraidIdx == 0 || BraidIdx == 3) ? 12 : 4;

		for (size_t SectionIndex = 1; SectionIndex < Sections.size() - 1; SectionIndex++)
		{
			if (Cancelled && Cancelled->load(std::memory_order_relaxed))
				return;

			swMatrix33 Transform1 = swMatrix33(Sections[SectionIndex]);
			swMatrix33 Transform2 = swMatrix33(Sections[SectionIndex + 1]);
			swVector3 Offset1 = Sections[SectionIndex].GetTranslation();
			swVector3 Offset2 = Sections[SectionIndex + 1].GetTranslation();

			for (size_t SegmentIdx = 0; SegmentIdx < ((SectionIndex < Sections.size() - 2) ? NumSegments : NumSegments + 1); SegmentIdx++)
			{
				float t = (float)SegmentIdx / (float)NumSegments;

				swVector3 Vertex1 = swVector3(t * 4.0f, PositionTable[(BaseX + SegmentIdx) % NumSegments], PositionTable[(BaseY + SegmentIdx) % NumSegments]) + swVector3(0.0f, 1.5f, 0.0f);
				swVector3 Vertex2 = swVector3((1.0f - t) * -4.0f, PositionTable[(BaseX + SegmentIdx) % NumSegments], PositionTable[(BaseY + SegmentIdx) % NumSegments]) + swVector3(0.0f, 1.5f, 0.0f);

				swVector3 Vertex = (swMultiply(Vertex1, Transform1) + Offset1) * (1.0f - t) + (swMultiply(Vertex2, Transform2) + Offset2) * t;

				VertexBuffer->Position = Vertex;
				VertexBuffer->Normal = swVector3(0.0f, 0.0f, 0.0f);
				VertexBuffer->NormalWeight = 0.0f;
				VertexBuffer++;

				if (SegmentIdx != NumSegments)
				{
					*IndexBuffer++ = BaseVertex;
					*IndexBuffer++ = BaseVertex + 1;
					BaseVertex++;
				}
			}
		}

		BaseVertex++;
	}

	int NumSlices = 16;
	MeshData.AddVertices(SW_MESH_DATA_SHARED, NumSlices * ((Sections.size() - 2) * NumSegments + 1), &BaseVertex, &VertexBuffer);
	MeshData.AddIndices(SW_MESH_DATA_SHARED, SW_MESH_TRIANGLES, 16, NumSlices * (Sections.size() - 2) * NumSegments * 6, &IndexBuffer);

	for (size_t SectionIndex = 1; SectionIndex < Sections.size() - 1; SectionIndex++)
	{
		if (Cancelled && Cancelled->load(std::memory_order_relaxed))
			return;

		swMatrix33 Transform1 = swMatrix33(Sections[SectionIndex]);
		swMatrix33 Transform2 = swMatrix33(Sections[SectionIndex + 1]);
		swVector3 Offset1 = Sections[SectionIndex].GetTranslation();
		swVector3 Offset2 = Sections[SectionIndex + 1].GetTranslation();

		for (size_t SegmentIdx = 0; SegmentIdx < ((SectionIndex < Sections.size() - 2) ? NumSegments : NumSegments + 1); SegmentIdx++)
		{
			float t1 = (float)SegmentIdx / (float)NumSegments;
			int BaseX = 8;
			int BaseY = 4;

			for (int SliceIdx = 0; SliceIdx < NumSlices; SliceIdx++)
			{
				swVector3 Vertex11 = swVector3(t1 * 4.0f, PositionTable[(BaseX + SliceIdx) % NumSlices], PositionTable[(BaseY + SliceIdx) % NumSlices]) + swVector3(0.0f, 1.5f, 0.0f);
				swVector3 Vertex12 = swVector3((1.0f - t1) * -4.0f, PositionTable[(BaseX + SliceIdx) % NumSlices], PositionTable[(BaseY + SliceIdx) % NumSlices]) + swVector3(0.0f, 1.5f, 0.0f);
				VertexBuffer->Position = (swMultiply(Vertex11, Transform1) + Offset1) * (1.0f - t1) + (swMultiply(Vertex12, Transform2) + Offset2) * t1;

				swVector3 Normal11 = swVector3(0.0f, PositionTable[(BaseX + SliceIdx) % NumSlices] / 1.5f, PositionTable[(BaseY + SliceIdx) % NumSlices] / 1.5f);
				swVector3 Normal12 = swVector3(0.0f, PositionTable[(BaseX + SliceIdx) % NumSlices] / 1.5f, PositionTable[(BaseY + SliceIdx) % NumSlices] / 1.5f);
				VertexBuffer->Normal = swMultiply(Normal11, Transform1) * (1.0f - t1) + swMultiply(Normal12, Transform2) * t1;
				VertexBuffer->NormalWeight = 1.0f;
				VertexBuffer++;

				if (SegmentIdx != NumSegments)
				{
					*IndexBuffer++ = BaseVertex + SliceIdx;
					*IndexBuffer++ = BaseVertex + (SliceIdx + 1) % NumSlices;
					*IndexBuffer++ = BaseVertex + (SliceIdx + 1) % NumSlices + NumSlices;

					*IndexBuffer++ = BaseVertex + SliceIdx + NumSlices;
					*IndexBuffer++ = BaseVertex + SliceIdx;
					*IndexBuffer++ = BaseVertex + (SliceIdx + 1) % NumSlices + NumSlices;
				}
			}

			BaseVertex += NumSlices;
		}
	}

	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 Transform(Sections[SectionIndex]);
		swVector3 Offset = swMultiply31(swVector3(8.0f, 0.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f 572A.dat\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2]);

		File.WriteBuffer(Line, strlen(Line));
	}
}

void swSynthesisInformationShockAbsorber::AddParts(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool*) const
{
	char Line[256];
	swVector3 Offset;

	Offset = Sections[0].GetTranslation();
	snprintf(Line, sizeof(Line), "1 0 %f %f %f 1 0 0 0 1 0 0 0 1 4254.dat\n", Offset[0], Offset[1], Offset[2]);
	File.WriteBuffer(Line, strlen(Line));

	Offset = Sections[1].GetTranslation();
	snprintf(Line, sizeof(Line), "1 16 %f %f %f 1 0 0 0 1 0 0 0 1 4255.dat\n", Offset[0], Offset[1], Offset[2]);
	File.WriteBuffer(Line, strlen(Line));

	float Distance = Sections[0].GetTranslation().y - Sections[1].GetTranslation().y;
	float Scale = (Distance - 66.0f) / 44.0f;

	Offset = Sections[0].GetTranslation();
	snprintf(Line, sizeof(Line), "1 494 %f %f %f 1 0 0 0 %f 0 0 0 1 %s\n", Offset[0], Offset[1] - 10 - 44.0f * Scale, Offset[2], Scale, mSpringPart);
	File.WriteBuffer(Line, strlen(Line));
}

void swSynthesisInformationActuator::AddParts(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool*) const
{
	char Line[256];
	swVector3 Offset;

	Offset = Sections[0].GetTranslation();
	snprintf(Line, sizeof(Line), "1 16 %f %f %f 1 0 0 0 1 0 0 0 1 %s\n", Offset[0], Offset[1], Offset[2], mBodyPart);
	File.WriteBuffer(Line, strlen(Line));

	Offset = swMultiply(Sections[0], swMatrix44Translation(swVector3(0.0f, 0.0f, mAxleOffset))).GetTranslation();
	snprintf(Line, sizeof(Line), "1 25 %f %f %f 0 1 0 -1 0 0 0 0 1 %s\n", Offset[0], Offset[1], Offset[2], mAxlePart);
	File.WriteBuffer(Line, strlen(Line));

	Offset = Sections[1].GetTranslation();
	snprintf(Line, sizeof(Line), "1 72 %f %f %f 1 0 0 0 1 0 0 0 1 %s\n", Offset[0], Offset[1], Offset[2], mPistonPart);
	File.WriteBuffer(Line, strlen(Line));
}

void swSynthesisInformationUniversalJoint::AddParts(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool*) const
{
	char Line[256];
	swVector3 Offset = Sections[0].GetTranslation();

	float Angle = atan2f(Offset.x, Offset.z);
	swMatrix44 Rotation = swMatrix44RotationZ(Angle);
	swMatrix44 Transform = swMatrix44StudWorksToLDraw(Rotation);

	snprintf(Line, sizeof(Line), "1 16 0 0 0 %f %f %f %f %f %f %f %f %f %s\n", Transform[0][0], Transform[1][0], Transform[2][0],
	         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mCenterPart);
	File.WriteBuffer(Line, strlen(Line));

	Angle = atan2f(Offset.y, hypotf(Offset.x, Offset.z));
	Rotation = swMultiply(Rotation, swMatrix44RotationX(Angle));
	Transform = swMultiply(swMatrix44Translation(swVector3(0.0f, 0.0f, mEndOffset)), swMatrix44StudWorksToLDraw(Rotation));

	snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Transform[3][0], Transform[3][1], Transform[3][2],
	         Transform[0][0], Transform[1][0], Transform[2][0], Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPart);
	File.WriteBuffer(Line, strlen(Line));

	Rotation = swMatrix44FromEulerAngles(swVector3(0.0f, SW_PI/2, SW_PI));
	Transform = swMatrix44StudWorksToLDraw(swMultiply(swMatrix44Translation(swVector3(0.0f, mEndOffset, 0.0f)), Rotation));
	snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Transform[3][0], Transform[3][1], Transform[3][2],
	         Transform[0][0], Transform[1][0], Transform[2][0], Transform[0][1], Transform[1][1], -Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPart);
	File.WriteBuffer(Line, strlen(Line));
}

std::unique_ptr<swMesh> swSynthesisInformation::BuildMesh(const std::vector<swPartControlPoint>& ControlPoints, const std::atomic_bool& Cancelled) const
{
	if (Cancelled.load(std::memory_order_relaxed))
		return nullptr;

	std::vector<swMatrix44> Sections;

	CalculateSections(ControlPoints, Sections, nullptr, &Cancelled);

	if (Cancelled.load(std::memory_order_relaxed))
		return nullptr;

	swLibraryMeshData MeshData;
	swMemoryFile File; // todo: rewrite this to pass the parts directly

	AddParts(File, MeshData, Sections, &Cancelled);

	if (Cancelled.load(std::memory_order_relaxed))
		return nullptr;

	File.WriteU8(0);
	File.Seek(0, SEEK_SET);

	swMeshLoader MeshLoader(MeshData, nullptr, swMeshLoaderFlag::RequireAllIncludes);
	MeshLoader.SetCancellationFlag(&Cancelled);

	if (MeshLoader.LoadMesh(File, SW_MESH_DATA_SHARED) && !MeshData.IsEmpty() && !Cancelled.load(std::memory_order_relaxed))
		return std::unique_ptr<swMesh>(MeshData.CreateMesh(&Cancelled));

	return nullptr;
}

// use to list the library parts the path is built from
std::vector<std::pair<std::string, swMatrix44>> swSynthesisInformation::GetParts(const std::vector<swPartControlPoint>& ControlPoints) const
{
	std::vector<std::pair<std::string, swMatrix44>> Parts;
	std::vector<swMatrix44> Sections;

	CalculateSections(ControlPoints, Sections, nullptr, nullptr);

	swLibraryMeshData MeshData;
	swMemoryFile File;

	AddParts(File, MeshData, Sections, nullptr);
	File.Seek(0, SEEK_SET);

	char Line[1024];

	while (File.ReadLine(Line, sizeof(Line)))
	{
		std::optional<std::pair<std::string, swMatrix44>> Reference = swShadowParser::ParseReference(Line);

		if (Reference)
			Parts.emplace_back(std::move(*Reference));
	}

	return Parts;
}

int swSynthesisInformation::InsertControlPoint(std::vector<swPartControlPoint>& ControlPoints, const swVector3& Start, const swVector3& End) const
{
	std::vector<swMatrix44> Sections;

	quint32 BestSegment = UINT32_MAX;
	float BestTime;
	float BestDistance = FLT_MAX;
	swVector3 BestPosition;
	swMatrix44 BestSection;

	CalculateSections(ControlPoints, Sections,
		[&](const swVector3& CurvePoint, int SegmentIndex, float t)
		{
			float Distance = swRayPointDistance(swVector3LDrawToStudWorks(CurvePoint), Start, End);
			if (Distance < BestDistance)
			{
				BestSegment = SegmentIndex;
				BestTime = t;
				BestDistance = Distance;
				BestPosition = swVector3LDrawToStudWorks(CurvePoint);
				BestSection = Sections.back();
			}
		}, nullptr
	);

	if (BestSegment != UINT32_MAX)
	{
		swPartControlPoint ControlPoint = ControlPoints[BestSegment];
		ControlPoint.Transform = swMatrix44(GetSectionControlPointRotation(BestSection, swMatrix33(ControlPoint.Transform)), BestPosition);

		if (BestSegment != ControlPoints.size() - 1)
		{
			swPartControlPoint NextControlPoint = ControlPoints[BestSegment + 1];
			ControlPoint.Scale = ControlPoint.Scale * (1.0f - BestTime) + NextControlPoint.Scale * BestTime;
		}

		ControlPoints.insert(ControlPoints.begin() + BestSegment + 1, ControlPoint);
	}

	return BestSegment + 1;
}
