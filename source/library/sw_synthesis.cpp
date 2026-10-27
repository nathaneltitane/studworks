#include "sw_global.h"
#include "sw_synthesis.h"
#include "sw_library.h"
#include "sw_application.h"
#include "sw_file.h"
#include "sw_loader_mesh.h"
#include "sw_mesh.h"
#include "sw_data_piece.h"
#include <locale.h>

class swSynthesisInformationCurved : public swSynthesisInformation
{
public:
	swSynthesisInformationCurved(float Length, float DefaultScale, int NumSections, bool RigidEdges);

	void GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;
	void VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;

protected:
	float GetSectionTwist(const swMatrix44& StartTransform, const swMatrix44& EndTransform) const;
	void CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
	static void AddTubePieces(swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, float Radius, bool IsInner, const std::atomic_bool* Cancelled);

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
	swSynthesisInformationFlexibleHose(float Length, int NumSections, const char* EdgePiece2);

protected:
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mEdgePiece2;
};

class swSynthesisInformationFlexSystemHose : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationFlexSystemHose(float Length, int NumSections);

	void GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;

protected:
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationPneumaticTube : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationPneumaticTube(float Length, int NumSections, const char* EndPiece);

protected:
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mEndPiece;
};

class swSynthesisInformationRibbedHose : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationRibbedHose(float Length, int NumSections);

protected:
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationFlexibleAxle : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationFlexibleAxle(float Length, int NumSections);

protected:
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationBraidedString : public swSynthesisInformationCurved
{
public:
	swSynthesisInformationBraidedString(float Length, int NumSections);

protected:
	void CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationStraight : public swSynthesisInformation
{
public:
	explicit swSynthesisInformationStraight(float Length);

	void VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;

protected:
	void CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
};

class swSynthesisInformationShockAbsorber : public swSynthesisInformationStraight
{
public:
	explicit swSynthesisInformationShockAbsorber(const char* SpringPiece);

	void GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;

protected:
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mSpringPiece;
};

class swSynthesisInformationActuator : public swSynthesisInformationStraight
{
public:
	explicit swSynthesisInformationActuator(const char* BodyPiece, const char* PistonPiece, const char* AxlePiece, float Length, float AxleOffset);

	void GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;

protected:
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	const char* mBodyPiece;
	const char* mPistonPiece;
	const char* mAxlePiece;
	float mAxleOffset;
};

class swSynthesisInformationUniversalJoint : public swSynthesisInformation
{
public:
	swSynthesisInformationUniversalJoint(float Length, float EndOffset, const char* EndPiece, const char* CenterPiece);

	void GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;
	void VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const override;

protected:
	void CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const override;
	void AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const override;

	float mEndOffset;
	const char* mEndPiece;
	const char* mCenterPiece;
};

void swSynthesisInitialize()
{
	swPiecesLibrary* Library = swGetPiecesLibrary();

	static const struct
	{
		char PieceID[16];
		float Length;
		int NumSections;
		char EdgePiece2[8];
	}
	FlexibleHoses[] =
	{
		{ "73590a.dat", 140.0f, 51, "752.dat" }, // Hose Flexible  8.5L without Tabs
		{ "73590b.dat", 140.0f, 51, "750.dat" }, // Hose Flexible  8.5L with Tabs
	};

	for (const auto& HoseInfo: FlexibleHoses)
	{
		swPieceData* PieceData = Library->FindPieceData(HoseInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationFlexibleHose(HoseInfo.Length, HoseInfo.NumSections, HoseInfo.EdgePiece2));
	}

	static const struct
	{
		char PieceID[16];
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
		swPieceData* PieceData = Library->FindPieceData(HoseInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationFlexSystemHose(HoseInfo.Length, HoseInfo.NumSections));
	}

	static const struct
	{
		char PieceID[16];
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
		swPieceData* PieceData = Library->FindPieceData(TubeInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationPneumaticTube(TubeInfo.Length, TubeInfo.NumSections, "71533k02.dat"));

		auto RegularInfo = TubeInfo;
		RegularInfo.PieceID[7] = '2';
		RegularInfo.Length -= 40.0f;

		PieceData = Library->FindPieceData(RegularInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationPneumaticTube(RegularInfo.Length, RegularInfo.NumSections, "71533k01.dat"));
	}

	static const struct
	{
		char PieceID[16];
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
		swPieceData* PieceData = Library->FindPieceData(HoseInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationRibbedHose(HoseInfo.Length, HoseInfo.NumSections));
	}

	static const struct
	{
		char PieceID[16];
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
		swPieceData* PieceData = Library->FindPieceData(AxleInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationFlexibleAxle(AxleInfo.Length, AxleInfo.NumSections));
	}

	static const struct
	{
		char PieceID[16];
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
		swPieceData* PieceData = Library->FindPieceData(StringInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationBraidedString(StringInfo.Length, StringInfo.NumSections));
	}

	static const struct
	{
		char PieceID[16];
		char SpringPiece[16];
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
		swPieceData* PieceData = Library->FindPieceData(AbsorberInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationShockAbsorber(AbsorberInfo.SpringPiece));
	}

	static const struct
	{
		char PieceID[16];
		char BodyPiece[16];
		char PistonPiece[16];
		char AxlePiece[16];
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
		swPieceData* PieceData = Library->FindPieceData(ActuatorInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationActuator(ActuatorInfo.BodyPiece, ActuatorInfo.PistonPiece,
					ActuatorInfo.AxlePiece, ActuatorInfo.Length, ActuatorInfo.AxleOffset));
	}

	static const struct
	{
		char PieceID[16];
		float Length;
		float EndOffset;
		char EndPiece[16];
		char CenterPiece[16];
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
		swPieceData* PieceData = Library->FindPieceData(JointInfo.PieceID, nullptr);

		if (PieceData)
			PieceData->SetSynthInfo(new swSynthesisInformationUniversalJoint(JointInfo.Length, JointInfo.EndOffset, JointInfo.EndPiece, JointInfo.CenterPiece));
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

swSynthesisInformationFlexibleHose::swSynthesisInformationFlexibleHose(float Length, int NumSections, const char* EdgePiece2)
	: swSynthesisInformationCurved(Length, 12.f, NumSections, true),
	mEdgePiece2(EdgePiece2)
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

swSynthesisInformationPneumaticTube::swSynthesisInformationPneumaticTube(float Length, int NumSections, const char* EndPiece)
	: swSynthesisInformationCurved(Length, 12.f, NumSections, true),
	mEndPiece(EndPiece)
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

swSynthesisInformationShockAbsorber::swSynthesisInformationShockAbsorber(const char* SpringPiece)
	: swSynthesisInformationStraight(110.00f), mSpringPiece(SpringPiece)
{
}

swSynthesisInformationActuator::swSynthesisInformationActuator(const char* BodyPiece, const char* PistonPiece, const char* AxlePiece, float Length, float AxleOffset)
	: swSynthesisInformationStraight(Length), mBodyPiece(BodyPiece), mPistonPiece(PistonPiece), mAxlePiece(AxlePiece), mAxleOffset(AxleOffset)
{
}

swSynthesisInformationUniversalJoint::swSynthesisInformationUniversalJoint(float Length, float EndOffset, const char* EndPiece, const char* CenterPiece)
	: swSynthesisInformation(Length), mEndOffset(EndOffset), mEndPiece(EndPiece), mCenterPiece(CenterPiece)
{
	mNondirectional = true;
}

void swSynthesisInformationCurved::GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	ControlPoints.resize(2);

	float HalfLength = mLength / 2.0f;
	float Scale = swMinimum(mDefaultScale, HalfLength);

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(-HalfLength, 0.0f, 0.0f));
	ControlPoints[1].Transform = swMatrix44Translation(swVector3( HalfLength, 0.0f, 0.0f));

	ControlPoints[0].Scale = Scale;
	ControlPoints[1].Scale = Scale;
}

void swSynthesisInformationCurved::VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	if (ControlPoints.size() < 2)
		GetDefaultControlPoints(ControlPoints);
}

void swSynthesisInformationFlexSystemHose::GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	swSynthesisInformationCurved::GetDefaultControlPoints(ControlPoints);

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, -mLength));
	ControlPoints[1].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, 0.0f));
}

void swSynthesisInformationStraight::VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	if (ControlPoints.size() < 2)
		GetDefaultControlPoints(ControlPoints);
	else
		ControlPoints.resize(2);
}

void swSynthesisInformationShockAbsorber::GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	ControlPoints.resize(2);

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, -mLength));
	ControlPoints[1].Transform = swMatrix44Translation(swVector3(0.0f, 0.0f, 0.0f));

	ControlPoints[0].Scale = 1.0f;
	ControlPoints[1].Scale = 1.0f;
}

void swSynthesisInformationActuator::GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	ControlPoints.resize(2);

	ControlPoints[0].Transform = swMatrix44(swMatrix33(swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, 1.0f, 0.0f)), swVector3(0.0f, 0.0f, 0.0f));
	ControlPoints[1].Transform = swMatrix44(swMatrix33(swVector3(1.0f, 0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, 1.0f, 0.0f)), swVector3(0.0f, mLength, 0.0f));

	ControlPoints[0].Scale = 1.0f;
	ControlPoints[1].Scale = 1.0f;
}

void swSynthesisInformationUniversalJoint::GetDefaultControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
{
	ControlPoints.resize(1);
	float HalfLength = mLength / 2;

	ControlPoints[0].Transform = swMatrix44Translation(swVector3(0.0f, HalfLength, 0.0f));
	ControlPoints[0].Scale = 1.0f;
}

void swSynthesisInformationUniversalJoint::VerifyControlPoints(std::vector<swPieceControlPoint>& ControlPoints) const
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

void swSynthesisInformationCurved::CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
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
				StartTransform.SetTranslation(swMultiply30(swVector3(0.0f, mStart.Length, 0.0f), StartTransform) + StartTransform.GetTranslation());
				SectionLength = 0.0f;
			}
			else
				SectionLength = mStart.Length;

			Sections.emplace_back(StartTransform);
		}

		EndTransform = swMatrix44(swMultiply(swMultiply(swMatrix33(mEnd.Transform), swMatrix33(EndTransform)), swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), EndTransform.GetTranslation());

		SegmentControlPoints[0] = StartTransform.GetTranslation();
		SegmentControlPoints[1] = swMultiply31(swVector3(0.0f, ControlPoints[ControlPointIndex].Scale, 0.0f), StartTransform);
		SegmentControlPoints[2] = swMultiply31(swVector3(0.0f, -ControlPoints[ControlPointIndex + 1].Scale, 0.0f), EndTransform);
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
		swVector3 Position = swMultiply31(swVector3(0.0f, SectionLength, 0.0f), EndTransform);
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

void swSynthesisInformationBraidedString::CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
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

void swSynthesisInformationStraight::CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
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

void swSynthesisInformationUniversalJoint::CalculateSections(const std::vector<swPieceControlPoint>& ControlPoints, std::vector<swMatrix44>& Sections, SectionCallbackFunc SectionCallback, const std::atomic_bool* Cancelled) const
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

void swSynthesisInformationFlexibleHose::AddPieces(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
{
	if (Sections.empty())
		return;

	char Line[256];
	const int NumEdgePieces = 2;

	const swMatrix33 EdgeTransforms[2] =
	{
		swMatrix33(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, -1.0f, 0.0f), swVector3(1.0f, 0.0f, 0.0f)),
		swMatrix33(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f,  1.0f, 0.0f), swVector3(1.0f, 0.0f, 0.0f))
	};

	const char* EdgePieces[2] =
	{
		"755.dat", mEdgePiece2
	};

	for (int PieceIdx = 0; PieceIdx < NumEdgePieces; PieceIdx++)
	{
		const int SectionIndex = 0;
		swMatrix33 Transform(swMultiply(swMultiply(EdgeTransforms[PieceIdx], swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, -5.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgePieces[PieceIdx]);

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

		const char* Piece = SectionIndex != Sections.size() / 2 ? "754.dat" : "756.dat";

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], Piece);

		File.WriteBuffer(Line, strlen(Line));
	}

	for (int PieceIdx = 0; PieceIdx < NumEdgePieces; PieceIdx++)
	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 Transform(swMultiply(EdgeTransforms[PieceIdx], swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 5.0f - 2.56f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgePieces[PieceIdx]);

		File.WriteBuffer(Line, strlen(Line));
	}
}

void swSynthesisInformationCurved::AddTubePieces(swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, float Radius, bool IsInner, const std::atomic_bool* Cancelled)
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

void swSynthesisInformationFlexSystemHose::AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
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

	AddTubePieces(MeshData, Sections, 4.0f, false, Cancelled);
	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		return;

	AddTubePieces(MeshData, Sections, 2.0f, true, Cancelled);
}

void swSynthesisInformationPneumaticTube::AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
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
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPiece);

		File.WriteBuffer(Line, strlen(Line));
	}

	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 EdgeTransform(swVector3(0.0f, 0.0f, -1.0f), swVector3(0.0f, -1.0f, 0.0f), swVector3(1.0f, 0.0f, 0.0f));
		swMatrix33 Transform(swMultiply(swMultiply(EdgeTransform, swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 0.0f, 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPiece);

		File.WriteBuffer(Line, strlen(Line));
	}

	AddTubePieces(MeshData, Sections, 5.0f, false, Cancelled);
	if (Cancelled && Cancelled->load(std::memory_order_relaxed))
		return;

	AddTubePieces(MeshData, Sections, 3.0f, true, Cancelled);
}

void swSynthesisInformationRibbedHose::AddPieces(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
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

void swSynthesisInformationFlexibleAxle::AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& Sections, const std::atomic_bool* Cancelled) const
{
	if (Sections.empty())
		return;

	char Line[256];
	const int NumEdgePieces = 6;

	swMatrix33 EdgeTransforms[6] =
	{
		swMatrix33(swVector3(-1.0f, 0.0f, 0.0f), swVector3(0.0f, -5.0f, 0.0f), swVector3(0.0f, 0.0f,  1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
		swMatrix33(swVector3( 0.0f, 1.0f, 0.0f), swVector3(1.0f,  0.0f, 0.0f), swVector3(0.0f, 0.0f, -1.0f)),
	};

	const char* EdgePieces[6] =
	{
		"stud3a.dat", "s/faxle1.dat", "s/faxle2.dat", "s/faxle3.dat", "s/faxle4.dat", "s/faxle5.dat"
	};

	for (int PieceIdx = 0; PieceIdx < NumEdgePieces; PieceIdx++)
	{
		const int SectionIndex = 0;
		swMatrix33 Transform(swMultiply(swMultiply(EdgeTransforms[PieceIdx], swMatrix33Scale(swVector3(1.0f, -1.0f, 1.0f))), swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, -4.0f * (5 - PieceIdx), 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgePieces[PieceIdx]);

		File.WriteBuffer(Line, strlen(Line));
	}

	for (int PieceIdx = 0; PieceIdx < NumEdgePieces; PieceIdx++)
	{
		const size_t SectionIndex = Sections.size() - 1;
		swMatrix33 Transform(swMultiply(EdgeTransforms[PieceIdx], swMatrix33(Sections[SectionIndex])));
		swVector3 Offset = swMultiply31(swVector3(0.0f, 4.0f * (5 - PieceIdx), 0.0f), Sections[SectionIndex]);

		snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Offset[0], Offset[1], Offset[2], Transform[0][0], Transform[1][0], Transform[2][0],
		         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], EdgePieces[PieceIdx]);

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

void swSynthesisInformationBraidedString::AddPieces(swMemoryFile& File, swLibraryMeshData& MeshData, const std::vector<swMatrix44>& SectionsIn, const std::atomic_bool* Cancelled) const
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

void swSynthesisInformationShockAbsorber::AddPieces(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool*) const
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
	snprintf(Line, sizeof(Line), "1 494 %f %f %f 1 0 0 0 %f 0 0 0 1 %s\n", Offset[0], Offset[1] - 10 - 44.0f * Scale, Offset[2], Scale, mSpringPiece);
	File.WriteBuffer(Line, strlen(Line));
}

void swSynthesisInformationActuator::AddPieces(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool*) const
{
	char Line[256];
	swVector3 Offset;

	Offset = Sections[0].GetTranslation();
	snprintf(Line, sizeof(Line), "1 16 %f %f %f 1 0 0 0 1 0 0 0 1 %s\n", Offset[0], Offset[1], Offset[2], mBodyPiece);
	File.WriteBuffer(Line, strlen(Line));

	Offset = swMultiply(Sections[0], swMatrix44Translation(swVector3(0.0f, 0.0f, mAxleOffset))).GetTranslation();
	snprintf(Line, sizeof(Line), "1 25 %f %f %f 0 1 0 -1 0 0 0 0 1 %s\n", Offset[0], Offset[1], Offset[2], mAxlePiece);
	File.WriteBuffer(Line, strlen(Line));

	Offset = Sections[1].GetTranslation();
	snprintf(Line, sizeof(Line), "1 72 %f %f %f 1 0 0 0 1 0 0 0 1 %s\n", Offset[0], Offset[1], Offset[2], mPistonPiece);
	File.WriteBuffer(Line, strlen(Line));
}

void swSynthesisInformationUniversalJoint::AddPieces(swMemoryFile& File, swLibraryMeshData&, const std::vector<swMatrix44>& Sections, const std::atomic_bool*) const
{
	char Line[256];
	swVector3 Offset = Sections[0].GetTranslation();

	float Angle = atan2f(Offset.x, Offset.z);
	swMatrix44 Rotation = swMatrix44RotationZ(Angle);
	swMatrix44 Transform = swMatrix44StudWorksToLDraw(Rotation);

	snprintf(Line, sizeof(Line), "1 16 0 0 0 %f %f %f %f %f %f %f %f %f %s\n", Transform[0][0], Transform[1][0], Transform[2][0],
	         Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mCenterPiece);
	File.WriteBuffer(Line, strlen(Line));

	Angle = atan2f(Offset.y, hypotf(Offset.x, Offset.z));
	Rotation = swMultiply(Rotation, swMatrix44RotationX(Angle));
	Transform = swMultiply(swMatrix44Translation(swVector3(0.0f, 0.0f, mEndOffset)), swMatrix44StudWorksToLDraw(Rotation));

	snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Transform[3][0], Transform[3][1], Transform[3][2],
	         Transform[0][0], Transform[1][0], Transform[2][0], Transform[0][1], Transform[1][1], Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPiece);
	File.WriteBuffer(Line, strlen(Line));

	Rotation = swMatrix44FromEulerAngles(swVector3(0.0f, SW_PI/2, SW_PI));
	Transform = swMatrix44StudWorksToLDraw(swMultiply(swMatrix44Translation(swVector3(0.0f, mEndOffset, 0.0f)), Rotation));
	snprintf(Line, sizeof(Line), "1 16 %f %f %f %f %f %f %f %f %f %f %f %f %s\n", Transform[3][0], Transform[3][1], Transform[3][2],
	         Transform[0][0], Transform[1][0], Transform[2][0], Transform[0][1], Transform[1][1], -Transform[2][1], Transform[0][2], Transform[1][2], Transform[2][2], mEndPiece);
	File.WriteBuffer(Line, strlen(Line));
}

std::unique_ptr<swMesh> swSynthesisInformation::BuildMesh(const std::vector<swPieceControlPoint>& ControlPoints, const std::atomic_bool& Cancelled) const
{
	if (Cancelled.load(std::memory_order_relaxed))
		return nullptr;

	std::vector<swMatrix44> Sections;

	CalculateSections(ControlPoints, Sections, nullptr, &Cancelled);

	if (Cancelled.load(std::memory_order_relaxed))
		return nullptr;

	swLibraryMeshData MeshData;
	swMemoryFile File; // todo: rewrite this to pass the parts directly

	AddPieces(File, MeshData, Sections, &Cancelled);

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

int swSynthesisInformation::InsertControlPoint(std::vector<swPieceControlPoint>& ControlPoints, const swVector3& Start, const swVector3& End) const
{
	std::vector<swMatrix44> Sections;

	quint32 BestSegment = UINT32_MAX;
	float BestTime;
	float BestDistance = FLT_MAX;
	swVector3 BestPosition;

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
			}
		}, nullptr
	);

	if (BestSegment != UINT32_MAX)
	{
		swPieceControlPoint ControlPoint = ControlPoints[BestSegment];
		ControlPoint.Transform.SetTranslation(BestPosition);

		if (BestSegment != ControlPoints.size() - 1)
		{
			swPieceControlPoint NextControlPoint = ControlPoints[BestSegment + 1];
			ControlPoint.Scale = ControlPoint.Scale * (1.0f - BestTime) + NextControlPoint.Scale * BestTime;
		}

		ControlPoints.insert(ControlPoints.begin() + BestSegment + 1, ControlPoint);
	}

	return BestSegment + 1;
}
