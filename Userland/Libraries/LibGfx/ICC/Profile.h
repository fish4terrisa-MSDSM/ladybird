/*
 * Copyright (c) 2022-2023, Nico Weber <thakis@chromium.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Format.h>
#include <AK/HashMap.h>
#include <AK/NonnullRefPtr.h>
#include <AK/RefCounted.h>
#include <AK/Span.h>
#include <AK/URL.h>
#include <LibCrypto/Hash/MD5.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/CIELAB.h>
#include <LibGfx/ICC/DistinctFourCC.h>
#include <LibGfx/ICC/TagTypes.h>
#include <LibGfx/Vector3.h>

namespace Gfx::ICC {

URL device_manufacturer_url(DeviceManufacturer);
URL device_model_url(DeviceModel);

// ICC v4, 7.2.4 Profile version field
class Version {
public:
    Version() = default;
    Version(u8 major, u8 minor_and_bugfix)
        : m_major_version(major)
        , m_minor_and_bugfix_version(minor_and_bugfix)
    {
    }

    u8 major_version() const { return m_major_version; }
    u8 minor_version() const { return m_minor_and_bugfix_version >> 4; }
    u8 bugfix_version() const { return m_minor_and_bugfix_version & 0xf; }

    u8 minor_and_bugfix_version() const { return m_minor_and_bugfix_version; }

private:
    u8 m_major_version = 0;
    u8 m_minor_and_bugfix_version = 0;
};

// ICC v4, 7.2.11 Profile flags field
class Flags {
public:
    Flags();

    // "The profile flags field contains flags."
    Flags(u32);

    u32 bits() const { return m_bits; }

    // "These can indicate various hints for the CMM such as distributed processing and caching options."
    // "The least-significant 16 bits are reserved for the ICC."
    u16 color_management_module_bits() const { return bits() >> 16; }
    u16 icc_bits() const { return bits() & 0xffff; }

    // "Bit position 0: Embedded profile (0 if not embedded, 1 if embedded in file)"
    bool is_embedded_in_file() const { return (icc_bits() & 1) != 0; }

    // "Bit position 1: Profile cannot be used independently of the embedded colour data (set to 1 if true, 0 if false)"
    // Double negation isn't unconfusing, so this function uses the inverted, positive sense.
    bool can_be_used_independently_of_embedded_color_data() const { return (icc_bits() & 2) == 0; }

    static constexpr u32 KnownBitsMask = 3;

private:
    u32 m_bits = 0;
};

// ICC v4, 7.2.14 Device attributes field
class DeviceAttributes {
public:
    DeviceAttributes();

    // "The device attributes field shall contain flags used to identify attributes
    // unique to the particular device setup for which the profile is applicable."
    DeviceAttributes(u64);

    u64 bits() const { return m_bits; }

    // "The least-significant 32 bits of this 64-bit value are defined by the ICC. "
    u32 icc_bits() const { return bits() & 0xffff'ffff; }

    // "Notice that bits 0, 1, 2, and 3 describe the media, not the device."

    // "0": "Reflective (0) or transparency (1)"
    enum class MediaReflectivity {
        Reflective,
        Transparent,
    };
    MediaReflectivity media_reflectivity() const { return MediaReflectivity(icc_bits() & 1); }

    // "1": "Glossy (0) or matte (1)"
    enum class MediaGlossiness {
        Glossy,
        Matte,
    };
    MediaGlossiness media_glossiness() const { return MediaGlossiness((icc_bits() >> 1) & 1); }

    // "2": "Media polarity, positive (0) or negative (1)"
    enum class MediaPolarity {
        Positive,
        Negative,
    };
    MediaPolarity media_polarity() const { return MediaPolarity((icc_bits() >> 2) & 1); }

    // "3": "Colour media (0), black & white media (1)"
    enum class MediaColor {
        Colored,
        BlackAndWhite,
    };
    MediaColor media_color() const { return MediaColor((icc_bits() >> 3) & 1); }

    // "4 to 31": Reserved (set to binary zero)"

    // "32 to 63": "Use not defined by ICC (vendor specific"
    u32 vendor_bits() const { return bits() >> 32; }

    static constexpr u64 KnownBitsMask = 0xf;

private:
    u64 m_bits = 0;
};

struct ProfileHeader {
    u32 on_disk_size { 0 };
    Optional<PreferredCMMType> preferred_cmm_type;
    Version version;
    DeviceClass device_class {};
    ColorSpace data_color_space {};
    ColorSpace connection_space {};
    time_t creation_timestamp { 0 };
    Optional<PrimaryPlatform> primary_platform {};
    Flags flags;
    Optional<DeviceManufacturer> device_manufacturer;
    Optional<DeviceModel> device_model;
    DeviceAttributes device_attributes;
    RenderingIntent rendering_intent {};
    XYZ pcs_illuminant;
    Optional<Creator> creator;
    Optional<Crypto::Hash::MD5::DigestType> id;
};

class Profile : public RefCounted<Profile> {
public:
    static ErrorOr<NonnullRefPtr<Profile>> try_load_from_externally_owned_memory(ReadonlyBytes);
    static ErrorOr<NonnullRefPtr<Profile>> create(ProfileHeader const& header, OrderedHashMap<TagSignature, NonnullRefPtr<TagData>> tag_table);

    Optional<PreferredCMMType> preferred_cmm_type() const { return m_header.preferred_cmm_type; }
    Version version() const { return m_header.version; }
    DeviceClass device_class() const { return m_header.device_class; }
    ColorSpace data_color_space() const { return m_header.data_color_space; }

    // For non-DeviceLink profiles, always PCSXYZ or PCSLAB.
    ColorSpace connection_space() const { return m_header.connection_space; }

    u32 on_disk_size() const { return m_header.on_disk_size; }
    time_t creation_timestamp() const { return m_header.creation_timestamp; }
    Optional<PrimaryPlatform> primary_platform() const { return m_header.primary_platform; }
    Flags flags() const { return m_header.flags; }
    Optional<DeviceManufacturer> device_manufacturer() const { return m_header.device_manufacturer; }
    Optional<DeviceModel> device_model() const { return m_header.device_model; }
    DeviceAttributes device_attributes() const { return m_header.device_attributes; }
    RenderingIntent rendering_intent() const { return m_header.rendering_intent; }
    XYZ const& pcs_illuminant() const { return m_header.pcs_illuminant; }
    Optional<Creator> creator() const { return m_header.creator; }
    Optional<Crypto::Hash::MD5::DigestType> const& id() const { return m_header.id; }

    static Crypto::Hash::MD5::DigestType compute_id(ReadonlyBytes);

    template<typename Callback>
    void for_each_tag(Callback callback) const
    {
        for (auto const& tag : m_tag_table)
            callback(tag.key, tag.value);
    }

    template<FallibleFunction<TagSignature, NonnullRefPtr<TagData>> Callback>
    ErrorOr<void> try_for_each_tag(Callback&& callback) const
    {
        for (auto const& tag : m_tag_table)
            TRY(callback(tag.key, tag.value));
        return {};
    }

    Optional<TagData const&> tag_data(TagSignature signature) const
    {
        return m_tag_table.get(signature).map([](auto it) -> TagData const& { return *it; });
    }

    Optional<String> tag_string_data(TagSignature signature) const;

    size_t tag_count() const { return m_tag_table.size(); }

    // Only versions 2 and 4 are in use.
    bool is_v2() const { return version().major_version() == 2; }
    bool is_v4() const { return version().major_version() == 4; }

    // FIXME: The color conversion stuff should be in some other class.

    // Converts an 8-bits-per-channel color to the profile connection space.
    // The color's number of channels must match number_of_components_in_color_space(data_color_space()).
    // Do not call for DeviceLink or NamedColor profiles. (XXX others?)
    // Call connection_space() to find out the space the result is in.
    ErrorOr<FloatVector3> to_pcs(ReadonlyBytes) const;

    // Converts from the profile connection space to an 8-bits-per-channel color.
    // The notes on `to_pcs()` apply to this too.
    ErrorOr<void> from_pcs(Profile const& source_profile, FloatVector3, Bytes) const;

    ErrorOr<CIELAB> to_lab(ReadonlyBytes) const;

    ErrorOr<void> convert_image(Bitmap&, Profile const& source_profile) const;

    // Only call these if you know that this is an RGB matrix-based profile.
    XYZ const& red_matrix_column() const;
    XYZ const& green_matrix_column() const;
    XYZ const& blue_matrix_column() const;

private:
    Profile(ProfileHeader const& header, OrderedHashMap<TagSignature, NonnullRefPtr<TagData>> tag_table)
        : m_header(header)
        , m_tag_table(move(tag_table))
    {
    }

    XYZ const& xyz_data(TagSignature tag) const
    {
        auto const& data = *m_tag_table.get(tag).value();
        VERIFY(data.type() == XYZTagData::Type);
        return static_cast<XYZTagData const&>(data).xyz();
    }

    ErrorOr<void> check_required_tags();
    ErrorOr<void> check_tag_types();

    // FIXME: The color conversion stuff should be in some other class.
    ErrorOr<FloatVector3> to_pcs_a_to_b(TagData const& tag_data, ReadonlyBytes) const;
    ErrorOr<void> from_pcs_b_to_a(TagData const& tag_data, FloatVector3 const&, Bytes) const;

    ProfileHeader m_header;
    OrderedHashMap<TagSignature, NonnullRefPtr<TagData>> m_tag_table;
};

}

template<>
struct AK::Formatter<Gfx::ICC::Version> : Formatter<FormatString> {
    ErrorOr<void> format(FormatBuilder& builder, Gfx::ICC::Version const& version)
    {
        return Formatter<FormatString>::format(builder, "{}.{}.{}"sv, version.major_version(), version.minor_version(), version.bugfix_version());
    }
};
