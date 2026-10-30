#include "sw_global.h"
#include "sw_file_gif.h"

// six bits per channel
static constexpr int SW_GIF_BIN_BITS = 6;
static constexpr int SW_GIF_BIN_COUNT = 1 << (SW_GIF_BIN_BITS * 3);
static constexpr int SW_GIF_TRANSPARENT_INDEX = 255;

struct swGifBin
{
	int Key;
	quint64 Count;
	quint64 Red;
	quint64 Green;
	quint64 Blue;
};

struct swGifBox
{
	int Begin;
	int End;
	int Channel;
	int Range;
};

static int swGetGifBinKey(QRgb Color)
{
	const int Shift = 8 - SW_GIF_BIN_BITS;

	return ((qRed(Color) >> Shift) << (SW_GIF_BIN_BITS * 2)) | ((qGreen(Color) >> Shift) << SW_GIF_BIN_BITS) | (qBlue(Color) >> Shift);
}

static int swGetGifBinChannel(int Key, int Channel)
{
	return (Key >> (SW_GIF_BIN_BITS * (2 - Channel))) & ((1 << SW_GIF_BIN_BITS) - 1);
}

// use to find the widest channel of a box
static void swMeasureGifBox(swGifBox& Box, const std::vector<swGifBin>& Bins)
{
	int Minimum[3] = { INT_MAX, INT_MAX, INT_MAX };
	int Maximum[3] = { 0, 0, 0 };

	for (int BinIndex = Box.Begin; BinIndex < Box.End; BinIndex++)
	{
		for (int Channel = 0; Channel < 3; Channel++)
		{
			const int Value = swGetGifBinChannel(Bins[BinIndex].Key, Channel);
			Minimum[Channel] = qMin(Minimum[Channel], Value);
			Maximum[Channel] = qMax(Maximum[Channel], Value);
		}
	}

	Box.Channel = 0;
	Box.Range = 0;

	for (int Channel = 0; Channel < 3; Channel++)
	{
		if (Maximum[Channel] - Minimum[Channel] > Box.Range)
		{
			Box.Channel = Channel;
			Box.Range = Maximum[Channel] - Minimum[Channel];
		}
	}
}

swGifWriter::swGifWriter(const QString& FileName, const QSize& Size)
	: mFile(FileName), mSize(Size)
{
}

bool swGifWriter::Open()
{
	if (!mFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

	QByteArray Data("GIF89a");

	// screen without a global palette
	AppendWord(Data, mSize.width());
	AppendWord(Data, mSize.height());
	Data.append('\0');
	Data.append('\0');
	Data.append('\0');

	// loop forever
	Data.append("\x21\xFF\x0B" "NETSCAPE2.0" "\x03\x01", 16);
	AppendWord(Data, 0);
	Data.append('\0');

	return mFile.write(Data) == Data.size();
}

bool swGifWriter::AddFrame(const QImage& Image, int Delay, bool Transparent)
{
	QImage Frame = Image.convertToFormat(QImage::Format_ARGB32);

	if (Frame.size() != mSize)
		Frame = Frame.scaled(mSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	const int PixelCount = mSize.width() * mSize.height();
	std::vector<int> Lookup(SW_GIF_BIN_COUNT, -1);
	std::vector<swGifBin> Bins;

	// count colors in bins
	for (int Y = 0; Y < mSize.height(); Y++)
	{
		const QRgb* Line = reinterpret_cast<const QRgb*>(Frame.constScanLine(Y));

		for (int X = 0; X < mSize.width(); X++)
		{
			const QRgb Color = Line[X];

			if (Transparent && qAlpha(Color) < 128)
				continue;

			const int Key = swGetGifBinKey(Color);

			if (Lookup[Key] < 0)
			{
				Lookup[Key] = static_cast<int>(Bins.size());
				Bins.push_back({ Key, 0, 0, 0, 0 });
			}

			swGifBin& Bin = Bins[Lookup[Key]];
			Bin.Count++;
			Bin.Red += qRed(Color);
			Bin.Green += qGreen(Color);
			Bin.Blue += qBlue(Color);
		}
	}

	// median cut the bins
	const int ColorCount = Transparent ? SW_GIF_TRANSPARENT_INDEX : 256;
	std::vector<swGifBox> Boxes;

	if (!Bins.empty())
	{
		Boxes.push_back({ 0, static_cast<int>(Bins.size()), 0, 0 });
		swMeasureGifBox(Boxes.back(), Bins);
	}

	while (static_cast<int>(Boxes.size()) < ColorCount)
	{
		int Widest = -1;

		for (int BoxIndex = 0; BoxIndex < static_cast<int>(Boxes.size()); BoxIndex++)
		{
			if (Boxes[BoxIndex].Range > 0 && (Widest < 0 || Boxes[BoxIndex].Range > Boxes[Widest].Range))
				Widest = BoxIndex;
		}

		if (Widest < 0)
			break;

		swGifBox Box = Boxes[Widest];
		const int Channel = Box.Channel;

		std::sort(Bins.begin() + Box.Begin, Bins.begin() + Box.End, [Channel](const swGifBin& First, const swGifBin& Second)
		{
			return swGetGifBinChannel(First.Key, Channel) < swGetGifBinChannel(Second.Key, Channel);
		});

		// split at the pixel median
		quint64 Total = 0;

		for (int BinIndex = Box.Begin; BinIndex < Box.End; BinIndex++)
			Total += Bins[BinIndex].Count;

		quint64 Running = 0;
		int Split = Box.Begin + 1;

		for (int BinIndex = Box.Begin; BinIndex < Box.End - 1; BinIndex++)
		{
			Running += Bins[BinIndex].Count;
			Split = BinIndex + 1;

			if (Running * 2 >= Total)
				break;
		}

		swGifBox Upper = { Split, Box.End, 0, 0 };
		Box.End = Split;

		swMeasureGifBox(Box, Bins);
		swMeasureGifBox(Upper, Bins);

		Boxes[Widest] = Box;
		Boxes.push_back(Upper);
	}

	// average each box into the palette
	QByteArray Palette(256 * 3, '\0');
	std::vector<QRgb> Colors;

	for (const swGifBox& Box : Boxes)
	{
		quint64 Count = 0, Red = 0, Green = 0, Blue = 0;

		for (int BinIndex = Box.Begin; BinIndex < Box.End; BinIndex++)
		{
			Count += Bins[BinIndex].Count;
			Red += Bins[BinIndex].Red;
			Green += Bins[BinIndex].Green;
			Blue += Bins[BinIndex].Blue;
		}

		const QRgb Color = qRgb(static_cast<int>((Red + Count / 2) / Count), static_cast<int>((Green + Count / 2) / Count), static_cast<int>((Blue + Count / 2) / Count));
		const int ColorIndex = static_cast<int>(Colors.size());

		Palette[ColorIndex * 3] = static_cast<char>(qRed(Color));
		Palette[ColorIndex * 3 + 1] = static_cast<char>(qGreen(Color));
		Palette[ColorIndex * 3 + 2] = static_cast<char>(qBlue(Color));
		Colors.push_back(Color);
	}

	// map each bin to its nearest color
	for (const swGifBin& Bin : Bins)
	{
		const int Red = static_cast<int>(Bin.Red / Bin.Count);
		const int Green = static_cast<int>(Bin.Green / Bin.Count);
		const int Blue = static_cast<int>(Bin.Blue / Bin.Count);
		int Nearest = 0;
		int NearestDistance = INT_MAX;

		for (int ColorIndex = 0; ColorIndex < static_cast<int>(Colors.size()); ColorIndex++)
		{
			const int DeltaRed = qRed(Colors[ColorIndex]) - Red;
			const int DeltaGreen = qGreen(Colors[ColorIndex]) - Green;
			const int DeltaBlue = qBlue(Colors[ColorIndex]) - Blue;
			const int Distance = DeltaRed * DeltaRed * 2 + DeltaGreen * DeltaGreen * 4 + DeltaBlue * DeltaBlue * 3;

			if (Distance < NearestDistance)
			{
				Nearest = ColorIndex;
				NearestDistance = Distance;
			}
		}

		Lookup[Bin.Key] = Nearest;
	}

	std::vector<quint8> Indices(PixelCount);

	for (int Y = 0; Y < mSize.height(); Y++)
	{
		const QRgb* Line = reinterpret_cast<const QRgb*>(Frame.constScanLine(Y));
		quint8* Index = Indices.data() + Y * mSize.width();

		for (int X = 0; X < mSize.width(); X++)
		{
			if (Transparent && qAlpha(Line[X]) < 128)
				Index[X] = SW_GIF_TRANSPARENT_INDEX;
			else
				Index[X] = static_cast<quint8>(Lookup[swGetGifBinKey(Line[X])]);
		}
	}

	QByteArray Data;

	// clear frames before the next one
	const int Disposal = Transparent ? 2 : 1;
	Data.append("\x21\xF9\x04", 3);
	Data.append(static_cast<char>((Disposal << 2) | (Transparent ? 1 : 0)));
	AppendWord(Data, qBound(2, Delay, 65535));
	Data.append(static_cast<char>(Transparent ? SW_GIF_TRANSPARENT_INDEX : 0));
	Data.append('\0');

	// full frame with its own palette
	Data.append('\x2C');
	AppendWord(Data, 0);
	AppendWord(Data, 0);
	AppendWord(Data, mSize.width());
	AppendWord(Data, mSize.height());
	Data.append('\x87');
	Data.append(Palette);

	AppendImageData(Data, Indices);

	return mFile.write(Data) == Data.size();
}

bool swGifWriter::Close()
{
	const bool Written = mFile.write("\x3B", 1) == 1;
	mFile.close();

	return Written && mFile.error() == QFileDevice::NoError;
}

void swGifWriter::AppendWord(QByteArray& Data, int Value) const
{
	Data.append(static_cast<char>(Value & 0xFF));
	Data.append(static_cast<char>((Value >> 8) & 0xFF));
}

// use to compress indices with lzw
void swGifWriter::AppendImageData(QByteArray& Data, const std::vector<quint8>& Indices) const
{
	const int MinimumCodeSize = 8;
	const int ClearCode = 1 << MinimumCodeSize;
	const int EndCode = ClearCode + 1;

	QByteArray Codes;
	quint32 BitBuffer = 0;
	int BitCount = 0;
	int CodeSize = MinimumCodeSize + 1;
	int LastCode = EndCode;
	std::unordered_map<quint32, int> Dictionary;
	Dictionary.reserve(4096);

	auto WriteCode = [&](int Code)
	{
		BitBuffer |= static_cast<quint32>(Code) << BitCount;
		BitCount += CodeSize;

		while (BitCount >= 8)
		{
			Codes.append(static_cast<char>(BitBuffer & 0xFF));
			BitBuffer >>= 8;
			BitCount -= 8;
		}
	};

	WriteCode(ClearCode);

	if (!Indices.empty())
	{
		int Prefix = Indices[0];

		for (size_t PixelIndex = 1; PixelIndex < Indices.size(); PixelIndex++)
		{
			const quint8 Value = Indices[PixelIndex];
			const quint32 Key = (static_cast<quint32>(Prefix) << 8) | Value;
			const std::unordered_map<quint32, int>::const_iterator Found = Dictionary.find(Key);

			if (Found != Dictionary.end())
			{
				Prefix = Found->second;
				continue;
			}

			WriteCode(Prefix);
			Dictionary.emplace(Key, ++LastCode);

			if (LastCode >= (1 << CodeSize))
				CodeSize++;

			// restart a full dictionary
			if (LastCode == 4095)
			{
				WriteCode(ClearCode);
				Dictionary.clear();
				CodeSize = MinimumCodeSize + 1;
				LastCode = EndCode;
			}

			Prefix = Value;
		}

		WriteCode(Prefix);
	}

	WriteCode(EndCode);

	if (BitCount > 0)
		Codes.append(static_cast<char>(BitBuffer & 0xFF));

	// sub blocks of up to 255 bytes
	Data.append(static_cast<char>(MinimumCodeSize));

	for (int Offset = 0; Offset < Codes.size(); Offset += 255)
	{
		const int Length = qMin(255, Codes.size() - Offset);
		Data.append(static_cast<char>(Length));
		Data.append(Codes.constData() + Offset, Length);
	}

	Data.append('\0');
}
