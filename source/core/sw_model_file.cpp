#include "sw_global.h"
#include "sw_model.h"
#include "sw_model_history.h"
#include "sw_object_part.h"
#include "sw_object_camera.h"
#include "sw_object_light.h"
#include "sw_group.h"
#include "sw_project.h"
#include "sw_window_main.h"
#include "sw_profile.h"
#include "sw_library.h"
#include "sw_scene.h"
#include "sw_track_train.h"
#include "sw_file.h"
#include "sw_data_part.h"
#include "sw_view.h"
#include "sw_builder_minifigure.h"
#include "sw_dialog_array.h"
#include "sw_dialog_group.h"
#include "sw_dialog_groups_edit.h"
#include "sw_dialog_properties.h"
#include "sw_window_utilities.h"
#include "sw_lxf.h"
#include "sw_widget_preview.h"
#include "sw_string.h"
#include "sw_command_meta.h"
#include <locale.h>

// Meta commands reach the parsers object first.
static bool swIsMetaCommand(const QString& Token)
{
	return Token == QLatin1String("!SW");
}

void swModelProperties::LoadDefaults()
{
	mAuthor = swGetProfileString(SW_PROFILE_DEFAULT_AUTHOR_NAME);
	mAmbientColor = swVector3FromColor(swGetProfileUnsignedInteger(SW_PROFILE_DEFAULT_AMBIENT_COLOR));
}

void swModelProperties::SaveDefaults()
{
	swSetProfileUnsignedInteger(SW_PROFILE_DEFAULT_AMBIENT_COLOR, swColorFromVector3(mAmbientColor));
}

void swModelProperties::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");

	Stream << QLatin1String("0 ") << mDescription << LineEnding;
	Stream << QLatin1String("0 Name: ") << mModelName << LineEnding;
	Stream << QLatin1String("0 Author: ") << mAuthor << LineEnding;

	if (!mComments.isEmpty())
	{
		const QStringList Comments = mComments.split('\n');
		for (const QString& Comment : Comments)
			Stream << QLatin1String("0 !SW COMMENT MODEL ") << Comment << LineEnding;
	}

//	swVector3 mAmbientColor;
}

bool swModelProperties::ParseLDrawHeader(QString Line, bool FirstLine)
{
	QTextStream LineStream(&Line, QIODevice::ReadOnly);

	QString Token;
	LineStream >> Token;
	const int StartPos = LineStream.pos();
	LineStream >> Token;

	if (swIsMetaCommand(Token))
		return false;

	if (Token == QLatin1String("Name:"))
	{
		mModelName = LineStream.readLine().mid(1);
		return true;
	}

	if (Token == QLatin1String("Author:"))
	{
		mAuthor = LineStream.readLine().mid(1);
		return true;
	}

	if (FirstLine)
	{
		LineStream.seek(StartPos);
		mDescription = LineStream.readLine().mid(1);
		return true;
	}

	return false;
}

void swModelProperties::ParseLDrawLine(QTextStream& Stream)
{
	QString Token;
	Stream >> Token;

	if (Token == QLatin1String("AUTHOR"))
		mAuthor = Stream.readLine().mid(1);
	else if (Token == QLatin1String("DESCRIPTION"))
		mDescription = Stream.readLine().mid(1);
	else if (Token == QLatin1String("COMMENT"))
	{
		QString Comment = Stream.readLine().mid(1);
		if (!mComments.isEmpty())
			mComments += '\n';
		mComments += Comment;
	}
}

void swPOVRayOptions::ParseLDrawLine(QTextStream& LineStream)
{
	QString Token;
	LineStream >> Token;

	if (Token == QLatin1String("HEADER_FILE_INCLUDE"))
	{
		LineStream >> HeaderIncludeFile;
		if (!QFileInfo(HeaderIncludeFile).isReadable())
			HeaderIncludeFile.clear();
	}
	else if (Token == QLatin1String("FOOTER_FILE_INCLUDE"))
	{
		LineStream >> FooterIncludeFile;
		if (!QFileInfo(FooterIncludeFile).isReadable())
			FooterIncludeFile.clear();
	}
	else if (Token == QLatin1String("FLOOR_AXIS"))
	{
		LineStream >> FloorAxis;
		if (FloorAxis < 0 || FloorAxis > 2)
			FloorAxis = 1; // y
	}
	else if (Token == QLatin1String("FLOOR_COLOR_RGB"))
	{
		swVector3 Color;
		LineStream >> Color[0] >> Color[1] >> Color[2];
		FloorColor = Color;
	}
	else if (Token == QLatin1String("FLOOR_AMBIENT"))
	{
		float Ambient;
		LineStream >> Ambient;
		FloorAmbient = Ambient;
	}
	else if (Token == QLatin1String("FLOOR_DIFFUSE"))
	{
		float Diffuse;
		LineStream >> Diffuse;
		FloorDiffuse = Diffuse;
	}
	else if (Token == QLatin1String("EXCLUDE_FLOOR"))
		ExcludeFloor = true;
	else if (Token == QLatin1String("EXCLUDE_BACKGROUND"))
		ExcludeBackground = true;
	else if (Token == QLatin1String("NO_REFLECTION"))
		NoReflection = true;
	else if (Token == QLatin1String("NO_SHADOWS"))
		NoShadow = true;
	else if (Token == QLatin1String("USE_LGEO"))
		UseLGEO = true;
}

void swPOVRayOptions::SaveLDraw(QTextStream& Stream) const
{
	const QLatin1String LineEnding("\r\n");
	if (!HeaderIncludeFile.isEmpty())
		Stream << QLatin1String("0 !SW HEADER_FILE_INCLUDE POV_RAY ") << QDir::toNativeSeparators(HeaderIncludeFile) << LineEnding;
	if (!FooterIncludeFile.isEmpty())
		Stream << QLatin1String("0 !SW FOOTER_FILE_INCLUDE POV_RAY ") << QDir::toNativeSeparators(FooterIncludeFile) << LineEnding;
	if (FloorAxis != 1)
		Stream << QLatin1String("0 !SW FLOOR_AXIS POV_RAY ") << FloorAxis << LineEnding;
	if (FloorColor)
		Stream << QLatin1String("0 !SW FLOOR_COLOR_RGB POV_RAY ") << (*FloorColor)[0] << ' ' << (*FloorColor)[1] << ' ' << (*FloorColor)[2] << LineEnding;
	if (FloorAmbient)
		Stream << QLatin1String("0 !SW FLOOR_AMBIENT POV_RAY ") << *FloorAmbient << LineEnding;
	if (FloorDiffuse)
		Stream << QLatin1String("0 !SW FLOOR_DIFFUSE POV_RAY ") << *FloorDiffuse << LineEnding;
	if (ExcludeFloor)
		Stream << QLatin1String("0 !SW EXCLUDE_FLOOR POV_RAY") << LineEnding;
	if (ExcludeBackground)
		Stream << QLatin1String("0 !SW EXCLUDE_BACKGROUND POV_RAY") << LineEnding;
	if (NoReflection)
		Stream << QLatin1String("0 !SW NO_REFLECTION POV_RAY") << LineEnding;
	if (NoShadow)
		Stream << QLatin1String("0 !SW NO_SHADOWS POV_RAY") << LineEnding;
	if (UseLGEO)
		Stream << QLatin1String("0 !SW USE_LGEO POV_RAY") << LineEnding;
}

static QString swGetStepEndLine(const swModelProperties& Properties, swStep Step)
{
	const std::map<swStep, swStepView>::const_iterator StepView = Properties.mStepViews.find(Step);

	if (StepView == Properties.mStepViews.end())
		return QLatin1String("0 STEP\r\n");

	if (StepView->second.Mode == swStepViewMode::End)
		return QLatin1String("0 ROTSTEP END\r\n");

	const char* Modes[] = { "REL", "ABS", "ADD" };
	const swVector3& Angles = StepView->second.Angles;

	return QString("0 ROTSTEP %1 %2 %3 %4\r\n").arg(swFormatValue(Angles.x, 2), swFormatValue(Angles.y, 2), swFormatValue(Angles.z, 2), QLatin1String(Modes[static_cast<int>(StepView->second.Mode)]));
}

void swModel::SaveLDraw(QTextStream& Stream, bool SelectedOnly, swStep LastStep) const
{
	const QLatin1String LineEnding("\r\n");

	mProperties.SaveLDraw(Stream);
	mPOVRayOptions.SaveLDraw(Stream);

	for (const QString& Directory : mAssetSourceDirectories)
		Stream << QLatin1String("0 !SW ASSET_SOURCE MODEL ") << QString::fromLatin1(Directory.toUtf8().toBase64()) << LineEnding;

	if (!mAssetSourceDirectories.empty() && !mPreferCurrentAssetDirectory)
		Stream << QLatin1String("0 !SW ASSET_SOURCE_PRIORITY MODEL SOURCE") << LineEnding;

	// copied parts leave the instructions behind
	if (!SelectedOnly)
	{
		for (const QString& InstructionLine : mInstructionLines)
		{
			const int PropertyEnd = InstructionLine.indexOf(QLatin1Char(' '));
			const QString Property = PropertyEnd == -1 ? InstructionLine : InstructionLine.left(PropertyEnd);
			const QString Arguments = PropertyEnd == -1 ? QString() : InstructionLine.mid(PropertyEnd);

			Stream << QLatin1String("0 !SW ") << Property << QLatin1String(" INSTRUCTION") << Arguments << LineEnding;
		}
	}

	std::vector<swGroup*> CurrentGroups;
	swStep Step = 1;
	int CurrentLine = 0;
	int AddedSteps = 0;
	bool SavedStep = false;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		if (SelectedOnly && !Part->IsSelected())
			continue;

		while (Part->GetFileLine() > CurrentLine && CurrentLine < mFileLines.size())
		{
			QString Line = mFileLines[CurrentLine];
			QTextStream LineStream(&Line, QIODevice::ReadOnly);

			QString Token;
			LineStream >> Token;
			bool Skip = false;
			bool StepLine = false;

			if (Token == QLatin1String("0"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("STEP"))
				{
					if (Part->GetStepShow() > Step)
					{
						if (LastStep && Step >= LastStep)
						{
							SavedStep = true;
							break;
						}

						Step++;
						StepLine = true;
					}
					else
						Skip = true;
				}
			}

			if (!Skip)
			{
				Stream << (StepLine ? swGetStepEndLine(mProperties, Step - 1) : mFileLines[CurrentLine]);
				if (AddedSteps > 0)
					AddedSteps--;
			}
			CurrentLine++;
		}

		if (SavedStep || (LastStep && Part->GetStepShow() > LastStep))
			break;

		while (Part->GetStepShow() > Step)
		{
			Stream << swGetStepEndLine(mProperties, Step);
			AddedSteps++;
			Step++;
		}

		swGroup* PartGroup = Part->GetGroup();

		if (PartGroup)
		{
			if (CurrentGroups.empty() || (!CurrentGroups.empty() && PartGroup != CurrentGroups[CurrentGroups.size() - 1]))
			{
				std::deque<swGroup*> PartParents;

				for (swGroup* Group = PartGroup; Group; Group = Group->mGroup)
					PartParents.push_front(Group);

				std::deque<swGroup*>::iterator ParentsToAdd = PartParents.begin();

				while (!CurrentGroups.empty())
				{
					swGroup* Group = CurrentGroups.back();
					const std::deque<swGroup*>::iterator ParentFound = std::find(PartParents.begin(), PartParents.end(), Group);

					if (ParentFound == PartParents.end())
					{
						CurrentGroups.pop_back();
						Stream << QLatin1String("0 !SW END GROUP\r\n");
					}
					else
					{
						ParentsToAdd = ParentFound + 1;
						break;
					}
				}

				for (std::deque<swGroup*>::iterator ParentIt = ParentsToAdd; ParentIt != PartParents.end(); ParentIt++)
				{
					swGroup* Group = *ParentIt;
					CurrentGroups.emplace_back(Group);
					Stream << QLatin1String("0 !SW BEGIN GROUP ") << Group->mName << LineEnding;
				}
			}
		}
		else
		{
			while (CurrentGroups.size())
			{
				CurrentGroups.pop_back();
				Stream << QLatin1String("0 !SW END GROUP\r\n");
			}
		}

		if (Part->mPartData->GetSynthInfo())
		{
			Stream << QLatin1String("0 !SW BEGIN SYNTH_PATH\r\n");

			const std::vector<swPartControlPoint>& ControlPoints = Part->GetControlPoints();
			for (const swPartControlPoint& ControlPoint : ControlPoints)
			{
				Stream << QLatin1String("0 !SW CONTROL_POINT SYNTH_PATH");

				const float* FloatMatrix = ControlPoint.Transform.GetFloats();
				const float Numbers[13] = { FloatMatrix[12], -FloatMatrix[14], FloatMatrix[13], FloatMatrix[0], -FloatMatrix[8], FloatMatrix[4], -FloatMatrix[2], FloatMatrix[10], -FloatMatrix[6], FloatMatrix[1], -FloatMatrix[9], FloatMatrix[5], ControlPoint.Scale };

				for (int NumberIdx = 0; NumberIdx < 13; NumberIdx++)
					Stream << ' ' << swFormatValue(Numbers[NumberIdx], NumberIdx < 3 ? 4 : 6);

				if (ControlPoint.TangentClamped)
					Stream << QLatin1String(" TANGENT");

				Stream << LineEnding;
			}
		}

		Part->SaveLDraw(Stream);

		if (Part->mPartData->GetSynthInfo())
			Stream << QLatin1String("0 !SW END SYNTH_PATH\r\n");
	}

	while (!SavedStep && CurrentLine < mFileLines.size())
	{
		QString Line = mFileLines[CurrentLine];
		QTextStream LineStream(&Line, QIODevice::ReadOnly);

		QString Token;
		LineStream >> Token;
		bool Skip = false;
		bool StepLine = false;

		if (Token == QLatin1String("0"))
		{
			LineStream >> Token;

			if (Token == QLatin1String("STEP"))
			{
				if (AddedSteps-- > 0)
					Skip = true;
				else
				{
					if (LastStep && Step >= LastStep)
						break;

					Step++;
					StepLine = true;
				}
			}
		}

		if (!Skip)
			Stream << (StepLine ? swGetStepEndLine(mProperties, Step - 1) : mFileLines[CurrentLine]);
		CurrentLine++;
	}

	while (CurrentGroups.size())
	{
		CurrentGroups.pop_back();
		Stream << QLatin1String("0 !SW END GROUP\r\n");
	}

	for (const std::unique_ptr<swCamera>& Camera : mCameras)
		if (!SelectedOnly || Camera->IsSelected())
			Camera->SaveLDraw(Stream);

	for (const std::unique_ptr<swLight>& Light : mLights)
		if (!SelectedOnly || Light->IsSelected())
			Light->SaveLDraw(Stream);

	Stream.flush();
}

void swModel::LoadLDraw(QIODevice& Device, Project* Project)
{
	swPart* Part = nullptr;
	swCamera* Camera = nullptr;
	swLight* Light = nullptr;
	std::vector<swGroup*> CurrentGroups;
	std::vector<swPartControlPoint> ControlPoints;
	int CurrentStep = 1;
	swPartsLibrary* Library = swGetPartsLibrary();
	mHasDirectGeometry = false;

	mProperties.mAuthor.clear();
	mProperties.mDescription.clear();
	mProperties.mComments.clear();
	mProperties.mStepViews.clear();
	mAssetSourceDirectories.clear();
	mInstructionLines.clear();
	mPreferCurrentAssetDirectory = true;
	bool ReadingHeader = true;
	bool FirstLine = true;

	while (!Device.atEnd())
	{
		const qint64 Pos = Device.pos();
		QString OriginalLine = Device.readLine();
		QString Line = swReadCommandMeta(OriginalLine.trimmed());
		QTextStream LineStream(&Line, QIODevice::ReadOnly);

		QString Token;
		LineStream >> Token;

		if (Token == QLatin1String("0"))
		{
			LineStream >> Token;

			if (Token == QLatin1String("FILE"))
			{
				QString Name = LineStream.readAll().trimmed();

				if (mProperties.mFileName != Name)
				{
					Device.seek(Pos);
					break;
				}

				continue;
			}
			else if (Token == QLatin1String("NOFILE") || Token == QLatin1String("!DATA"))
			{
				break;
			}

			if (ReadingHeader)
			{
				ReadingHeader = mProperties.ParseLDrawHeader(Line, FirstLine);
				FirstLine = false;

				if (ReadingHeader)
					continue;
			}

			if (Token == QLatin1String("STEP") || Token == QLatin1String("ROTSTEP"))
			{
				// ROTSTEP ends the step like STEP and sets its view rotation.
				if (Token == QLatin1String("ROTSTEP"))
				{
					swStepView StepView;
					QString Value;
					LineStream >> Value;

					if (Value.compare(QLatin1String("END"), Qt::CaseInsensitive) == 0)
						StepView.Mode = swStepViewMode::End;
					else
					{
						QString Mode;
						StepView.Angles.x = Value.toFloat();
						LineStream >> StepView.Angles.y >> StepView.Angles.z >> Mode;

						if (Mode.compare(QLatin1String("ABS"), Qt::CaseInsensitive) == 0)
							StepView.Mode = swStepViewMode::Absolute;
						else if (Mode.compare(QLatin1String("ADD"), Qt::CaseInsensitive) == 0)
							StepView.Mode = swStepViewMode::Add;
					}

					mProperties.mStepViews[CurrentStep] = StepView;
					OriginalLine = QLatin1String(OriginalLine.endsWith(QLatin1String("\r\n")) ? "0 STEP\r\n" : "0 STEP\n");
				}

				delete Part;
				Part = nullptr;
				CurrentStep++;
				mFileLines.append(OriginalLine);
				continue;
			}

			if (!swIsMetaCommand(Token))
			{
				mFileLines.append(OriginalLine);

				if (Token == QLatin1String("!:"))
				{
					LineStream >> Token;

					if (Token == QLatin1String("1") || Token == QLatin1String("2") || Token == QLatin1String("3") || Token == QLatin1String("4") || Token == QLatin1String("5"))
						mHasDirectGeometry = true;
				}

				continue;
			}

			LineStream >> Token;

			if (Token == QLatin1String("MODEL"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("ASSET_SOURCE"))
				{
					const QString Directory = QString::fromUtf8(QByteArray::fromBase64(LineStream.readAll().trimmed().toLatin1()));

					if (QDir::isAbsolutePath(Directory))
					{
						const QString CleanDirectory = QDir::cleanPath(Directory);

						if (std::find(mAssetSourceDirectories.begin(), mAssetSourceDirectories.end(), CleanDirectory) == mAssetSourceDirectories.end())
							mAssetSourceDirectories.push_back(CleanDirectory);
					}
				}
				else if (Token == QLatin1String("ASSET_SOURCE_PRIORITY"))
				{
					LineStream >> Token;
					mPreferCurrentAssetDirectory = Token != QLatin1String("SOURCE");
				}
				else
				{
					QString PropertiesLine = Token + LineStream.readAll();
					QTextStream PropertiesStream(&PropertiesLine, QIODevice::ReadOnly);
					mProperties.ParseLDrawLine(PropertiesStream);
				}
			}
			else if (Token == QLatin1String("PART"))
			{
				if (!Part)
					Part = new swPart(nullptr);

				Part->ParseLDrawLine(LineStream);
			}
			else if (Token == QLatin1String("CAMERA"))
			{
				if (!Camera)
					Camera = new swCamera(false);

				if (Camera->ParseLDrawLine(LineStream))
				{
					Camera->CreateName(mCameras);
					mCameras.emplace_back(Camera);
					Camera = nullptr;
				}
			}
			else if (Token == QLatin1String("LIGHT"))
			{
				if (!Light)
					Light = new swLight(swVector3(0.0f, 0.0f, 0.0f), swLightType::Point);

				if (Light->ParseLDrawLine(LineStream))
				{
					Light->CreateName(mLights);
					mLights.emplace_back(Light);
					Light = nullptr;
				}
			}
			else if (Token == QLatin1String("POV_RAY"))
			{
				mPOVRayOptions.ParseLDrawLine(LineStream);
			}
			else if (Token == QLatin1String("INSTRUCTION"))
			{
				mInstructionLines.append(LineStream.readAll().trimmed());
			}
			else if (Token == QLatin1String("GROUP"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("BEGIN"))
				{
					QString Name = LineStream.readAll().trimmed();
					swGroup* Group = GetGroup(Name, true);
					if (!CurrentGroups.empty())
						Group->mGroup = CurrentGroups[CurrentGroups.size() - 1];
					else
						Group->mGroup = nullptr;
					CurrentGroups.emplace_back(Group);
				}
				else if (Token == QLatin1String("END"))
				{
					if (!CurrentGroups.empty())
						CurrentGroups.pop_back();
				}
			}
			else if (Token == QLatin1String("SYNTH_PATH"))
			{
				LineStream >> Token;

				if (Token == QLatin1String("BEGIN"))
				{
					ControlPoints.clear();
				}
				else if (Token == QLatin1String("END"))
				{
					ControlPoints.clear();
				}
				else if (Token == QLatin1String("CONTROL_POINT"))
				{
					float Numbers[13];
					for (int TokenIdx = 0; TokenIdx < 13; TokenIdx++)
						LineStream >> Numbers[TokenIdx];

					swPartControlPoint& PartControlPoint = ControlPoints.emplace_back();
					PartControlPoint.Transform = swMatrix44(swVector4(Numbers[3], Numbers[9], -Numbers[6], 0.0f), swVector4(Numbers[5], Numbers[11], -Numbers[8], 0.0f),
					                                         swVector4(-Numbers[4], -Numbers[10], Numbers[7], 0.0f), swVector4(Numbers[0], Numbers[2], -Numbers[1], 1.0f));
					PartControlPoint.Scale = Numbers[12];

					LineStream >> Token;
					PartControlPoint.TangentClamped = Token == QLatin1String("TANGENT");
				}
			}

			continue;
		}
		else if (Token == QLatin1String("1"))
		{
			ReadingHeader = false;
			int ColorCode;
			LineStream >> ColorCode;

			float IncludeMatrix[12];
			for (int TokenIdx = 0; TokenIdx < 12; TokenIdx++)
				LineStream >> IncludeMatrix[TokenIdx];

			swMatrix44 IncludeTransform(swVector4(IncludeMatrix[3], IncludeMatrix[6], IncludeMatrix[9], 0.0f), swVector4(IncludeMatrix[4], IncludeMatrix[7], IncludeMatrix[10], 0.0f),
										swVector4(IncludeMatrix[5], IncludeMatrix[8], IncludeMatrix[11], 0.0f), swVector4(IncludeMatrix[0], IncludeMatrix[1], IncludeMatrix[2], 1.0f));

			QString PartId = LineStream.readAll().trimmed();

			if (PartId.isEmpty())
				continue;

			QByteArray CleanId = PartId.toLatin1().toUpper().replace('\\', '/');

			if (Library->FindPrimitive(CleanId.constData()))
			{
				mFileLines.append(OriginalLine);
				mHasDirectGeometry = true;
			}
			else
			{
				if (!Part)
					Part = new swPart(nullptr);

				if (!CurrentGroups.empty())
					Part->SetGroup(CurrentGroups[CurrentGroups.size() - 1]);

				swPartDataReference PartData = Library->AcquirePartData(PartId.toLatin1().constData(), Project, true, true);

				const float* Matrix = IncludeTransform.GetFloats();
				const swMatrix44 Transform(swVector4(Matrix[0], Matrix[2], -Matrix[1], 0.0f), swVector4(Matrix[8], Matrix[10], -Matrix[9], 0.0f),
									       swVector4(-Matrix[4], -Matrix[6], Matrix[5], 0.0f), swVector4(Matrix[12], Matrix[14], -Matrix[13], 1.0f));

				Part->SetFileLine(mFileLines.size());
				Part->SetPartData(std::move(PartData), PartId, false, false);
				Part->Initialize(Transform, CurrentStep);
				Part->SetColorCode(ColorCode);
				Part->VerifyControlPoints(ControlPoints);
				Part->SetControlPoints(ControlPoints);
				ControlPoints.clear();

				if (Part->mPartData->IsModel() && Part->mPartData->GetModel()->IncludesModel(this))
				{
					Project->AddLoadWarning(tr("Model '%1': reference to '%2' was ignored because it would create a circular reference.").arg(GetFileName(), PartId));
					delete Part;
					Part = nullptr;
					continue;
				}

				AddPart(Part);
				Part = nullptr;
			}
		}
		else
		{
			ReadingHeader = false;
			mFileLines.append(OriginalLine);

			if (Token == QLatin1String("2") || Token == QLatin1String("3") || Token == QLatin1String("4") || Token == QLatin1String("5"))
				mHasDirectGeometry = true;
		}

		FirstLine = false;
	}

	mCurrentStep = CurrentStep;
	CalculateStep(mCurrentStep);

	delete Part;
	delete Camera;
	delete Light;
}

bool swModel::LoadBinary(swFile* file)
{
	qint32 i, count;
	char id[32];
	quint32 rgb;
	float fv = 0.4f;
	quint8 ch;
	quint16 sh;

	file->Seek(0, SEEK_SET);
	file->ReadBuffer(id, 32);
	sscanf(&id[7], "%f", &fv);

	if (memcmp(id, "LeoCAD ", 7))
		return false;

	if (fv == 0.0f)
	{
		const lconv *loc = localeconv();
		id[8] = loc->decimal_point[0];
		sscanf(&id[7], "%f", &fv);

		if (fv == 0.0f)
			return false;
	}

	if (fv > 0.4f)
		file->ReadFloats(&fv, 1);

	file->ReadU32(&rgb, 1);

	if (fv < 0.6f) // old view
	{
		double eye[3], target[3];
		file->ReadDoubles(eye, 3);
		file->ReadDoubles(target, 3);
	}

	file->Seek(28, SEEK_CUR);
	file->ReadS32(&i, 1);
	mCurrentStep = i;

	if (fv > 0.8f)
		file->ReadU32();//m_nScene

	file->ReadS32(&count, 1);
	swPartsLibrary* Library = swGetPartsLibrary();

	const size_t FirstNewPart = mParts.size();

	while (count--)
	{
		if (fv > 0.4f)
		{
			swPart* pPart = new swPart(nullptr);
			pPart->FileLoad(*file, mProject);
			AddPart(pPart);
		}
		else
		{
			char name[SW_PART_NAME_LENGTH];
			swVector3 pos, rot;
			quint8 color, step, group;

			file->ReadFloats(pos.GetFloats(), 3);
			file->ReadFloats(rot.GetFloats(), 3);
			file->ReadU8(&color, 1);
			file->ReadBuffer(name, 9);
			swstrcat(name, ".dat");
			file->ReadU8(&step, 1);
			file->ReadU8(&group, 1);

			pos *= 25.0f;
			swMatrix44 WorldMatrix = swMultiply(swMatrix44RotationZ(rot[2] * SW_DEGREES_TO_RADIANS), swMultiply(swMatrix44RotationY(rot[1] * SW_DEGREES_TO_RADIANS), swMatrix44RotationX(rot[0] * SW_DEGREES_TO_RADIANS)));
			WorldMatrix.SetTranslation(pos);

			swPart* pPart = new swPart(Library->AcquirePartData(name, mProject, true, false));

			pPart->Initialize(WorldMatrix, step);
			pPart->SetColorCode(swGetColorCodeFromOriginalColor(color));
			AddPart(pPart);

//			pPart->SetGroup((swGroup*)group);
		}
	}

	if (fv >= 0.4f)
	{
		file->ReadBuffer(&ch, 1);
		if (ch == 0xFF) file->ReadU16(&sh, 1); else sh = ch;
		if (sh > 100)
			file->Seek(sh, SEEK_CUR);
		else
		{
			QByteArray Author;
			Author.resize(sh + 1);
			file->ReadBuffer(Author.data(), sh);
			Author[sh] = 0;
			mProperties.mAuthor = QString::fromUtf8(Author);
		}

		file->ReadBuffer(&ch, 1);
		if (ch == 0xFF) file->ReadU16(&sh, 1); else sh = ch;
		if (sh > 100)
			file->Seek(sh, SEEK_CUR);
		else
		{
			QByteArray Description;
			Description.resize(sh + 1);
			file->ReadBuffer(Description.data(), sh);
			Description[sh] = 0;
			mProperties.mDescription = QString::fromUtf8(Description);
		}

		file->ReadBuffer(&ch, 1);
		if (ch == 0xFF && fv < 1.3f) file->ReadU16(&sh, 1); else sh = ch;
		if (sh > 255)
			file->Seek(sh, SEEK_CUR);
		else
		{
			QByteArray Comments;
			Comments.resize(sh + 1);
			file->ReadBuffer(Comments.data(), sh);
			Comments[sh] = 0;
			mProperties.mComments = QString::fromUtf8(Comments);
			mProperties.mComments.replace(QLatin1String("\r\n"), QLatin1String("\n"));
		}
	}

	if (fv >= 0.5f)
	{
		const size_t NumGroups = mGroups.size();

		file->ReadS32(&count, 1);
		for (i = 0; i < count; i++)
			mGroups.emplace_back(new swGroup());

		for (size_t GroupIdx = NumGroups; GroupIdx < mGroups.size(); GroupIdx++)
		{
			swGroup* Group = mGroups[GroupIdx].get();

			if (fv < 1.0f)
			{
				char Name[SW_MAXIMUM_GROUP_NAME + 1];
				file->ReadBuffer(Name, sizeof(Name));
				Group->mName = QString::fromUtf8(Name);
				file->ReadBuffer(&ch, 1);
				Group->mGroup = (swGroup*)-1;
			}
			else
				Group->FileLoad(file);
		}

		for (size_t GroupIdx = NumGroups; GroupIdx < mGroups.size(); GroupIdx++)
		{
			swGroup* Group = mGroups[GroupIdx].get();

			i = (qint32)(quintptr)(Group->mGroup);
			Group->mGroup = nullptr;

			if (i > 0xFFFF || i == -1)
				continue;

			Group->mGroup = mGroups[NumGroups + i].get();
		}

		for (size_t PartIndex = FirstNewPart; PartIndex < mParts.size(); PartIndex++)
		{
			swPart* Part = mParts[PartIndex].get();

			i = (qint32)(quintptr)(Part->GetGroup());
			Part->SetGroup(nullptr);

			if (i > 0xFFFF || i == -1)
				continue;

			Part->SetGroup(mGroups[NumGroups + i].get());
		}

		RemoveEmptyGroups();
	}

	if (fv >= 0.6f)
	{
		if (fv < 1.0f)
			file->Seek(4, SEEK_CUR);
		else
			file->Seek(2, SEEK_CUR);

		file->ReadS32(&count, 1);

		for (i = 0; i < count; i++)
			swCamera::FileLoad(*file);
	}

	if (fv >= 0.7f)
	{
		file->Seek(24, SEEK_CUR);

		if (fv < 1.3f)
		{
			file->ReadU8(&ch, 1);
			if (ch == 0xFF)
				file->ReadU16(&sh, 1);
			sh = ch;
		}
		else
			file->ReadU16(&sh, 1);

		file->Seek(sh, SEEK_CUR); // Background
	}

	if (fv >= 0.8f)
	{
		file->ReadBuffer(&ch, 1);
		file->Seek(ch, SEEK_CUR);
		file->ReadBuffer(&ch, 1);
		file->Seek(ch, SEEK_CUR);
	}

	if (fv > 0.9f)
	{
		file->ReadU32(&rgb, 1);
		mProperties.mAmbientColor[0] = (float)((unsigned char) (rgb))/255;
		mProperties.mAmbientColor[1] = (float)((unsigned char) (((unsigned short) (rgb)) >> 8))/255;
		mProperties.mAmbientColor[2] = (float)((unsigned char) ((rgb) >> 16))/255;

		if (fv < 1.3f)
			file->Seek(23, SEEK_CUR);
		else
			file->Seek(11, SEEK_CUR);
	}

	if (fv > 1.0f)
	{
		file->ReadU32(&rgb, 1);
		file->ReadU32(&rgb, 1);
	}

	CalculateStep(mCurrentStep);

	return true;
}

bool swModel::LoadLDD(const QString& FileData)
{
	std::vector<swPart*> Parts;
	std::vector<std::vector<swPart*>> Groups;

	if (!swImportLXFMLFile(FileData, mProject, Parts, Groups))
		return false;

	for (swPart* Part : Parts)
		AddPart(Part);

	for (const std::vector<swPart*>& Group : Groups)
	{
		swGroup* NewGroup = AddGroup(tr("Group "), nullptr);
		for (swPart* Part : Group)
			Part->SetGroup(NewGroup);
	}

	CalculateStep(mCurrentStep);

	return true;
}

bool swModel::LoadInventory(const std::vector<swSetInventoryItem>& SetInventory)
{
	swPartsLibrary* Library = swGetPartsLibrary();

	for (const swSetInventoryItem& SetInventoryItem : SetInventory)
	{
		const swPartDataReference PartData = Library->AcquirePartData(SetInventoryItem.PartID + ".dat", mProject, true, false);
		int Quantity = SetInventoryItem.Quantity;

		while (Quantity--)
		{
			swPart* Part = new swPart(nullptr);
			Part->SetPartData(PartData, QString(), false, true);
			Part->Initialize(swMatrix44Identity(), 1);
			Part->SetColorCode(SetInventoryItem.ColorCode);
			AddPart(Part);
		}
	}

	if (mParts.empty())
		return false;

	std::vector<swPartDataReference> RequiredPartData;

	RequiredPartData.reserve(mParts.size());

	for (const std::unique_ptr<swPart>& Part : mParts)
		RequiredPartData.push_back(Part->mPartData);

	if (!Library->WaitForPartData(RequiredPartData))
		return false;

	auto RoundBounds = [](float& Value)
	{
		Value = ((Value < 0.0f) ? floor((Value - 5.0f) / 10.0f) : ceil((Value + 5.0f) / 10.0f)) * 10.0f;
	};

	constexpr float TargetHeight = 800.0f;
	float CurrentX = 0.0f;
	float CurrentY = 0.0f;
	float ColumnWidth = 0.0f;

	for (const std::unique_ptr<swPart>& Part : mParts)
	{
		swBoundingBox BoundingBox = Part->mPartData->GetBoundingBox();
		RoundBounds(BoundingBox.Min.x);
		RoundBounds(BoundingBox.Min.y);
		RoundBounds(BoundingBox.Max.x);
		RoundBounds(BoundingBox.Max.y);

		const float PartWidth = BoundingBox.Max.x - BoundingBox.Min.x;
		const float PartHeight = BoundingBox.Max.y - BoundingBox.Min.y;

		if (CurrentY + PartHeight > TargetHeight)
		{
			CurrentY = 0.0f;
			CurrentX += ColumnWidth;
			ColumnWidth = 0.0f;
		}

		Part->SetPosition(swVector3(CurrentX + PartWidth / 2.0f, CurrentY + PartHeight / 2.0f, 0.0f), 1, false);
		CurrentY += PartHeight;
		ColumnWidth = qMax(ColumnWidth, PartWidth);
	}

	CalculateStep(mCurrentStep);

	return true;
}
