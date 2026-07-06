#include "MachineFingerprint.h"

#include <cstring>

#if JUCE_MAC
 #include <CommonCrypto/CommonDigest.h>
#elif JUCE_WINDOWS
 #include <windows.h>
 #include <bcrypt.h>
 #pragma comment(lib, "bcrypt.lib")
#endif

namespace licensing
{
namespace
{
juce::String sha256Hex (const juce::MemoryBlock& data)
{
   #if JUCE_MAC
    unsigned char digest[CC_SHA256_DIGEST_LENGTH] {};
    CC_SHA256 (data.getData(), (CC_LONG) data.getSize(), digest);
   #elif JUCE_WINDOWS
    unsigned char digest[32] {};
    BCRYPT_ALG_HANDLE algorithmHandle = nullptr;
    DWORD hashObjectSize = 0;
    DWORD dataSize = 0;
    DWORD hashSize = 0;
    BCRYPT_HASH_HANDLE hashHandle = nullptr;

    if (BCryptOpenAlgorithmProvider (&algorithmHandle, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return {};

    BCryptGetProperty (algorithmHandle, BCRYPT_OBJECT_LENGTH, (PUCHAR) &hashObjectSize, sizeof (hashObjectSize), &dataSize, 0);
    BCryptGetProperty (algorithmHandle, BCRYPT_HASH_LENGTH, (PUCHAR) &hashSize, sizeof (hashSize), &dataSize, 0);
    juce::HeapBlock<unsigned char> hashObject ((size_t) hashObjectSize);

    if (BCryptCreateHash (algorithmHandle, &hashHandle, hashObject.getData(), hashObjectSize, nullptr, 0, 0) != 0)
    {
        BCryptCloseAlgorithmProvider (algorithmHandle, 0);
        return {};
    }

    BCryptHashData (hashHandle, (PUCHAR) data.getData(), (ULONG) data.getSize(), 0);
    BCryptFinishHash (hashHandle, digest, (ULONG) hashSize, 0);
    BCryptDestroyHash (hashHandle);
    BCryptCloseAlgorithmProvider (algorithmHandle, 0);
   #else
    juce::ignoreUnused (data);
    return {};
   #endif

    juce::String hex;

    for (auto byte : digest)
        hex << juce::String::toHexString ((int) byte).paddedLeft ('0', 2);

    return hex;
}
}

juce::String MachineFingerprint::createMachineIdHash()
{
    juce::StringArray parts;
    parts.add (juce::SystemStats::getOperatingSystemName());
    parts.add (juce::SystemStats::getComputerName());

    juce::Array<juce::MACAddress> macAddresses;
    juce::MACAddress::findAllAddresses (macAddresses);
    juce::StringArray normalisedAddresses;

    for (const auto& address : macAddresses)
    {
        const auto text = address.toString();

        if (text.isNotEmpty() && text != "00-00-00-00-00-00")
            normalisedAddresses.add (text);
    }

    normalisedAddresses.sort (true);
    parts.addArray (normalisedAddresses);

    const auto raw = parts.joinIntoString ("|");
    const auto* utf8 = raw.toRawUTF8();
    juce::MemoryBlock bytes (utf8, std::strlen (utf8));
    return sha256Hex (bytes);
}
}
