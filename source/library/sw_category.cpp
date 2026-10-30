#include "sw_global.h"
#include "sw_category.h"
#include "sw_string.h"
#include "sw_profile.h"

std::vector<swLibraryCategory> gCategories;

void swResetDefaultCategories()
{
	swResetCategories(gCategories);

	swRemoveProfileKey(SW_PROFILE_CATEGORIES);
}

void swLoadDefaultCategories(bool BuiltInLibrary)
{
	QByteArray Buffer = swGetProfileBuffer(SW_PROFILE_CATEGORIES);

	if (Buffer.isEmpty() || !swLoadCategories(Buffer, gCategories))
		swResetCategories(gCategories, BuiltInLibrary);
}

void swSaveDefaultCategories()
{
	QByteArray ByteArray;
	QTextStream Stream(&ByteArray, QIODevice::WriteOnly);

	swSaveCategories(Stream, gCategories);

	swSetProfileBuffer(SW_PROFILE_CATEGORIES, ByteArray);
}

void swResetCategories(std::vector<swLibraryCategory>& Categories, bool BuiltInLibrary)
{
	const char DefaultCategories[] =
	{
		"Animal=^%Animal | ^%Bone\n"
		"Antenna=^%Antenna\n"
		"Arch=^%Arch\n"
		"Bar=^%Bar\n"
		"Baseplate=^%Baseplate | ^%Platform\n"
		"Boat=^%Boat | ^%Sail\n"
		"Brick=^%Brick\n"
		"Container=^%Container | ^%Box | ^Chest | ^%Storage | ^Mailbox\n"
		"Door and Window=^%Door | ^%Window | ^%Glass | ^%Freestyle | ^%Gate | ^%Garage | ^%Roller\n"
		"Electric=^%Battery | ^%Electric\n"
		"Hinge and Bracket=^%Hinge | ^%Bracket | ^%Turntable\n"
		"Hose=^%Hose | ^%Rubber | ^%String\n"
		"Minifig=^%Minifig\n"
		"Miscellaneous=^%Arm | ^%Barrel | ^%Brush | ^%Bucket | ^%Cardboard | ^%Claw | ^%Cockpit | ^%Cocoon | ^%Conveyor | ^%Crane | ^%Cupboard | ^%Fence | ^%Gold | ^%Handle | ^%Hook | ^%Jack | ^%Key | ^%Ladder | ^%Medical | ^%Motor | ^%Rock | ^%Rope | ^%Slide | ^%Sheet | ^%Snow | ^%Sports | ^%Spring | ^%Staircase | ^%Stretcher | ^%Tap | ^%Tipper | ^%Trailer | ^%Umbrella | ^%Winch\n"
		"Other=^%Ball | ^%Belville | ^%BigFig | ^%Die | ^%Duplo | ^%Fabuland | ^%Figure | ^%Homemaker | ^%Maxifig | ^%Microfig | ^%Mursten | ^%Quatro | ^%Scala | ^%Znap\n"
		"Panel=^%Panel | ^%Castle Wall | ^%Castle Turret\n"
		"Plant=^%Plant\n"
		"Plate=^%Plate\n"
		"Round=^%Cylinder | ^%Cone | ^%Dish | ^%Dome | ^%Hemisphere | ^%Round\n"
		"Sign and Flag=^%Flag | ^%Roadsign | ^%Streetlight | ^%Flagpost | ^%Lamppost | ^%Signpost\n"
		"Slope=^%Slope | ^%Roof\n"
		"Sticker=^%Sticker\n"
		"Support=^%Support\n"
		"Technic=^%Technic | ^%Rack\n"
		"Tile=^%Tile\n"
		"Train=^%Train | ^%Monorail | ^%Magnet\n"
		"Tyre and Wheel=^%Tyre | %^Wheel | %^Wheels | ^%Castle Wagon | ^%Axle\n"
		"Vehicle=^%Bike | ^%Canvas | ^%Car | ^%Excavator | ^%Exhaust | ^%Forklift | ^%Grab Jaw | ^%Jet | ^%Landing | ^%Motorcycle | ^%Plane | ^%Propellor | ^%Tail | ^%Tractor | ^%Vehicle | ^%Wheelbarrow\n"
		"Windscreen=^%Windscreen\n"
		"Wedge=^%Wedge\n"
		"Wing=^%Wing\n"
	};

	const char BuiltInCategories[] =
	{
		"Baseplate=^%Baseplate\n"
		"Brick=^%Brick\n"
		"Plate=^%Plate\n"
		"Slope=^%Slope\n"
		"Tile=^%Tile\n"
	};

	QByteArray Buffer;

	if (BuiltInLibrary)
		Buffer.append(BuiltInCategories, sizeof(BuiltInCategories));
	else
		Buffer.append(DefaultCategories, sizeof(DefaultCategories));

	swLoadCategories(Buffer, Categories);
}

bool swLoadCategories(const QString& FileName, std::vector<swLibraryCategory>& Categories)
{
	QFile File(FileName);

	if (!File.open(QIODevice::ReadOnly))
		return false;

	QByteArray FileData = File.readAll();

	return swLoadCategories(FileData, Categories);
}

bool swLoadCategories(const QByteArray& Buffer, std::vector<swLibraryCategory>& Categories)
{
	Categories.clear();

	QTextStream Stream(Buffer);

	for (QString Line = Stream.readLine(); !Line.isNull(); Line = Stream.readLine())
	{
		qsizetype Equals = Line.indexOf('=');

		if (Equals == -1)
			continue;

		QString Name = Line.left(Equals);
		QString Keywords = Line.mid(Equals + 1);

		swLibraryCategory Category;

		// disabled categories start with a hash
		Category.Enabled = !Name.startsWith('#');
		Category.Name = Category.Enabled ? Name : Name.mid(1);
		Category.Keywords = Keywords.toLatin1();

		Categories.emplace_back(std::move(Category));
	}

	return true;
}

bool swSaveCategories(const QString& FileName, const std::vector<swLibraryCategory>& Categories)
{
	QFile File(FileName);

	if (!File.open(QIODevice::WriteOnly))
		return false;

	QTextStream Stream(&File);

	return swSaveCategories(Stream, Categories);
}

bool swSaveCategories(QTextStream& Stream, const std::vector<swLibraryCategory>& Categories)
{
	QString Format("%1=%2\r\n");

	for (const swLibraryCategory& Category : Categories)
		Stream << Format.arg(Category.Enabled ? Category.Name : '#' + Category.Name, QString::fromLatin1(Category.Keywords));

	Stream.flush();

	return true;
}

bool swMatchCategory(const char* PartName, const char* Expression)
{
	// split the test expression
	const char* p = Expression;

	while (*p)
	{
		if (*p == '!')
		{
			return !swMatchCategory(PartName, p + 1);
		}
		else if (*p == '(')
		{
//			const char* Start = p;
			int c = 0;

			// skip the parenthesis contents
			do
			{
				if (*p == '(')
						c++;
				else if (*p == ')')
						c--;
				else if (*p == 0)

					// mismatched parenthesis
					return false;

				p++;
			}
			while (c);

			if (*p == 0)
				break;
		}
		else if ((*p == '|') || (*p == '&'))
		{
			std::string LeftStr(Expression, std::max(p - Expression, static_cast<ptrdiff_t>(1)) - 1);
			std::string RightStr(p + 1);

			if (*p == '|')
				return swMatchCategory(PartName, LeftStr.c_str()) || swMatchCategory(PartName, RightStr.c_str());
			else
				return swMatchCategory(PartName, LeftStr.c_str()) && swMatchCategory(PartName, RightStr.c_str());
		}

		p++;
	}

	if (strchr(Expression, '('))
	{
		p = Expression;
		while (*p)
		{
			if (*p == '(')
			{
				const char* Start = p;
				int c = 0;

				// extract the parenthesis contents
				do
				{
					if (*p == '(')
							c++;
					else if (*p == ')')
							c--;
					else if (*p == 0)

						// mismatched parenthesis
						return false;

					p++;
				}
				while (c);

				std::string SubExpression(Start + 1, p - Start - 2);
				return swMatchCategory(PartName, SubExpression.c_str());
			}

			p++;
		}
	}

	const char* SearchStart = Expression;
	while (isspace(*SearchStart))
		SearchStart++;

	const char* SearchEnd = SearchStart + strlen(SearchStart) - 1;
	while (SearchEnd >= SearchStart && isspace(*SearchEnd))
		SearchEnd--;

	// simple case
	std::string Search;
	if (SearchStart != SearchEnd)
		Search = std::string(SearchStart, SearchEnd - SearchStart + 1);
	const char* Word = Search.c_str();

	// check for modifiers
	bool WholeWord = 0;
	bool Begin = 0;

	for (;;)
	{
		if (Word[0] == '^')
			WholeWord = true;
		else if (Word[0] == '%')
			Begin = true;
		else
			break;

		Word++;
	}

	if (!Word[0])
		return false;

	const char* Result = swstrcasestr(PartName, Word);

	if (!Result)
		return false;

	if (Begin && (Result != PartName))
	{
		if ((Result != PartName + 1) || ((Result[-1] != '_') && (Result[-1] != '~')))
			return false;
	}

	if (WholeWord)
	{
		char End = Result[strlen(Word)];

		if ((End != 0) && (End != ' '))
			return false;

		if ((Result != PartName) && ((Result[-1] == '_') || (Result[-1] == '~')))
			Result--;

		if ((Result != PartName) && (Result[-1] != ' '))
			return false;
	}

	return true;
}
