#include <STDInclude.hpp>
#include "IXAnimParts.hpp"

#define IW4X_ANIM_VERSION 1

namespace Assets
{
	void IXAnimParts::load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File animFile(std::format("xanim/{}.iw4xAnim", name));

		if (animFile.exists())
		{
			Utils::Stream::Reader reader(builder->getAllocator(), animFile.getBuffer());

			__int64 magic = reader.read<__int64>();
			if (std::memcmp(&magic, "IW4xAnim", 8))
			{
				Components::Logger::Error(Game::ERR_FATAL, "Reading animation '{}' failed, header is invalid!", name);
			}

			int version = reader.read<int>();
			if (version != IW4X_ANIM_VERSION)
			{
				Components::Logger::Error(Game::ERR_FATAL, "Reading animation '{}' failed, expected version is {}, but it was {}!", name, IW4X_ANIM_VERSION, version);
			}

			Game::XAnimParts* xanim = reader.readArray<Game::XAnimParts>();

			if (xanim)
			{
				if (xanim->name)
				{
					xanim->name = reader.readCString();
				}

				if (xanim->names)
				{
					xanim->names = builder->getAllocator()->allocateArray<unsigned short>(xanim->boneCount[Game::PART_TYPE_ALL]);
					for (int i = 0; i < xanim->boneCount[Game::PART_TYPE_ALL]; ++i)
					{
						xanim->names[i] = static_cast<std::uint16_t>(Game::SL_GetString(reader.readCString(), 0));
					}
				}

				if (xanim->notify)
				{
					xanim->notify = reader.readArray<Game::XAnimNotifyInfo>(xanim->notifyCount);

					for (int i = 0; i < xanim->notifyCount; ++i)
					{
						xanim->notify[i].name = static_cast<std::uint16_t>(Game::SL_GetString(reader.readCString(), 0));
					}
				}

				if (xanim->dataByte)
				{
					xanim->dataByte = reader.readArray<char>(xanim->dataByteCount);
				}

				if (xanim->dataShort)
				{
					xanim->dataShort = reader.readArray<short>(xanim->dataShortCount);
				}

				if (xanim->dataInt)
				{
					xanim->dataInt = reader.readArray<int>(xanim->dataIntCount);
				}

				if (xanim->randomDataByte)
				{
					xanim->randomDataByte = reader.readArray<char>(xanim->randomDataByteCount);
				}

				if (xanim->randomDataShort)
				{
					xanim->randomDataShort = reader.readArray<short>(xanim->randomDataShortCount);
				}

				if (xanim->randomDataInt)
				{
					xanim->randomDataInt = reader.readArray<int>(xanim->randomDataIntCount);
				}

				if (xanim->indices.data)
				{
					if (xanim->numframes < 256)
					{
						xanim->indices._1 = reader.readArray<char>(xanim->indexCount);
					}
					else
					{
						xanim->indices._2 = reader.readArray<unsigned short>(xanim->indexCount);
					}
				}

				if (!reader.end())
				{
					Components::Logger::Error(Game::ERR_FATAL, "Reading animation '{}' failed, remaining raw data found!", name);
				}

				header->parts = xanim;
			}
		}
	}

	void IXAnimParts::mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		Game::XAnimParts* asset = header.parts;

		if (asset->names)
		{
			for (char i = 0; i < asset->boneCount[Game::PART_TYPE_ALL]; ++i)
			{
				builder->addScriptString(asset->names[i]);
			}
		}

		if (asset->notify)
		{
			for (char i = 0; i < asset->notifyCount; ++i)
			{
				builder->addScriptString(asset->notify[i].name);
			}
		}
	}

	void IXAnimParts::saveXAnimDeltaPart(Game::XAnimDeltaPart* delta, unsigned short framecount, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::XAnimDeltaPart, 12);

		Utils::Stream* buffer = builder->getBuffer();
		Game::XAnimDeltaPart* destDelta = buffer->dest<Game::XAnimDeltaPart>();
		buffer->save(delta);

		if (delta->trans)
		{
			buffer->align(Utils::Stream::ALIGN_4);
			buffer->save(delta->trans, 4);

			if (delta->trans->size)
			{
				buffer->save(&delta->trans->u.frames, 28);

				if (framecount > 0xFF)
				{
					buffer->saveArray(delta->trans->u.frames.indices._2, delta->trans->size + 1);
				}
				else
				{
					buffer->saveArray(delta->trans->u.frames.indices._1, delta->trans->size + 1);
				}

				if (delta->trans->u.frames.frames._1)
				{
					if (delta->trans->smallTrans)
					{
						buffer->save(delta->trans->u.frames.frames._1, 3, delta->trans->size + 1);
					}
					else
					{
						buffer->align(Utils::Stream::ALIGN_4);
						buffer->save(delta->trans->u.frames.frames._1, 6, delta->trans->size + 1);
					}
				}
			}
			else
			{
				buffer->save(delta->trans->u.frame0, 12);
			}

			Utils::Stream::ClearPointer(&destDelta->trans);
		}

		if (delta->quat2)
		{
			buffer->align(Utils::Stream::ALIGN_4);
			buffer->save(delta->quat2, 4);

			if (delta->quat2->size)
			{
				buffer->save(&delta->quat2->u.frames, 4);

				if (framecount > 0xFF)
				{
					buffer->save(delta->quat2->u.frames.indices._1, 2, delta->quat2->size + 1);
				}
				else
				{
					buffer->save(delta->quat2->u.frames.indices._1, 1, delta->quat2->size + 1);
				}

				if (delta->quat2->u.frames.frames)
				{
					buffer->align(Utils::Stream::ALIGN_4);
					buffer->save(delta->quat2->u.frames.frames, 4, delta->quat2->size + 1);
				}
			}
			else
			{
				buffer->save(delta->quat2->u.frame0, 4);
			}

			Utils::Stream::ClearPointer(&destDelta->quat2);
		}

		if (delta->quat)
		{
			buffer->align(Utils::Stream::ALIGN_4);
			buffer->save(delta->quat, 4);

			if (delta->quat->size)
			{
				buffer->save(&delta->quat->u.frames, 4);

				if (framecount > 0xFF)
				{
					buffer->save(delta->quat->u.frames.indices._1, 2, delta->quat->size + 1);
				}
				else
				{
					buffer->save(delta->quat->u.frames.indices._1, 1, delta->quat->size + 1);
				}

				if (delta->quat->u.frames.frames)
				{
					buffer->align(Utils::Stream::ALIGN_4);
					buffer->save(delta->quat->u.frames.frames, 4, delta->quat->size + 1);
				}
			}
			else
			{
				buffer->save(delta->quat->u.frame0, 4);
			}

			Utils::Stream::ClearPointer(&destDelta->quat);
		}
	}

	void IXAnimParts::save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::XAnimParts, 88);

		Utils::Stream* buffer = builder->getBuffer();
		Game::XAnimParts* asset = header.parts;
		Game::XAnimParts* dest = buffer->dest<Game::XAnimParts>();
		buffer->save(asset, sizeof(Game::XAnimParts));

		buffer->pushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->saveString(builder->getAssetName(this->getType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->names)
		{
			buffer->align(Utils::Stream::ALIGN_2);

			unsigned short* destTagnames = buffer->dest<unsigned short>();
			buffer->saveArray(asset->names, asset->boneCount[Game::PART_TYPE_ALL]);

			for (char i = 0; i < asset->boneCount[Game::PART_TYPE_ALL]; ++i)
			{
				builder->mapScriptString(destTagnames[i]);
			}

			Utils::Stream::ClearPointer(&dest->names);
		}

		if (asset->notify)
		{
			AssertSize(Game::XAnimNotifyInfo, 8);
			buffer->align(Utils::Stream::ALIGN_4);

			Game::XAnimNotifyInfo* destNotetracks = buffer->dest<Game::XAnimNotifyInfo>();
			buffer->saveArray(asset->notify, asset->notifyCount);

			for (char i = 0; i < asset->notifyCount; ++i)
			{
				builder->mapScriptString(destNotetracks[i].name);
			}

			Utils::Stream::ClearPointer(&dest->notify);
		}

		if (asset->deltaPart)
		{
			AssertSize(Game::XAnimDeltaPart, 12);
			buffer->align(Utils::Stream::ALIGN_4);

			this->saveXAnimDeltaPart(asset->deltaPart, asset->numframes, builder);

			Utils::Stream::ClearPointer(&dest->deltaPart);
		}

		if (asset->dataByte)
		{
			buffer->saveArray(asset->dataByte, asset->dataByteCount);
			Utils::Stream::ClearPointer(&dest->dataByte);
		}

		if (asset->dataShort)
		{
			buffer->align(Utils::Stream::ALIGN_2);
			buffer->saveArray(asset->dataShort, asset->dataShortCount);
			Utils::Stream::ClearPointer(&dest->dataShort);
		}

		if (asset->dataInt)
		{
			buffer->align(Utils::Stream::ALIGN_4);
			buffer->saveArray(asset->dataInt, asset->dataIntCount);
			Utils::Stream::ClearPointer(&dest->dataInt);
		}

		if (asset->randomDataShort)
		{
			buffer->align(Utils::Stream::ALIGN_2);
			buffer->saveArray(asset->randomDataShort, asset->randomDataShortCount);
			Utils::Stream::ClearPointer(&dest->randomDataShort);
		}

		if (asset->randomDataByte)
		{
			buffer->saveArray(asset->randomDataByte, asset->randomDataByteCount);
			Utils::Stream::ClearPointer(&dest->randomDataByte);
		}

		if (asset->randomDataInt)
		{
			buffer->align(Utils::Stream::ALIGN_4);
			buffer->saveArray(asset->randomDataInt, asset->randomDataIntCount);
			Utils::Stream::ClearPointer(&dest->randomDataInt);
		}

		if (asset->indices.data)
		{
			if (asset->numframes > 0xFF)
			{
				buffer->align(Utils::Stream::ALIGN_2);
				buffer->saveArray(asset->indices._2, asset->indexCount);
			}
			else
			{
				buffer->saveArray(asset->indices._1, asset->indexCount);
			}

			Utils::Stream::ClearPointer(&dest->indices.data);
		}

		buffer->popBlock();
	}
}
