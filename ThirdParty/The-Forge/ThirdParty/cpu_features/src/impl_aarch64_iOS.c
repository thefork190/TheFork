// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpu_features_macros.h"

#if defined(ARCH_ARM64) || defined(TARGET_IOS_SIMULATOR)

#include "cpuinfo_aarch64.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions for introspection.
////////////////////////////////////////////////////////////////////////////////
#define INTROSPECTION_TABLE                                                \
  LINE(AARCH64_FP, fp, "fp", AARCH64_HWCAP_FP, 0)                          \
  LINE(AARCH64_ASIMD, asimd, "asimd", AARCH64_HWCAP_ASIMD, 0)              \
  LINE(AARCH64_EVTSTRM, evtstrm, "evtstrm", AARCH64_HWCAP_EVTSTRM, 0)      \
  LINE(AARCH64_AES, aes, "aes", AARCH64_HWCAP_AES, 0)                      \
  LINE(AARCH64_PMULL, pmull, "pmull", AARCH64_HWCAP_PMULL, 0)              \
  LINE(AARCH64_SHA1, sha1, "sha1", AARCH64_HWCAP_SHA1, 0)                  \
  LINE(AARCH64_SHA2, sha2, "sha2", AARCH64_HWCAP_SHA2, 0)                  \
  LINE(AARCH64_CRC32, crc32, "crc32", AARCH64_HWCAP_CRC32, 0)              \
  LINE(AARCH64_ATOMICS, atomics, "atomics", AARCH64_HWCAP_ATOMICS, 0)      \
  LINE(AARCH64_FPHP, fphp, "fphp", AARCH64_HWCAP_FPHP, 0)                  \
  LINE(AARCH64_ASIMDHP, asimdhp, "asimdhp", AARCH64_HWCAP_ASIMDHP, 0)      \
  LINE(AARCH64_CPUID, cpuid, "cpuid", AARCH64_HWCAP_CPUID, 0)              \
  LINE(AARCH64_ASIMDRDM, asimdrdm, "asimdrdm", AARCH64_HWCAP_ASIMDRDM, 0)  \
  LINE(AARCH64_JSCVT, jscvt, "jscvt", AARCH64_HWCAP_JSCVT, 0)              \
  LINE(AARCH64_FCMA, fcma, "fcma", AARCH64_HWCAP_FCMA, 0)                  \
  LINE(AARCH64_LRCPC, lrcpc, "lrcpc", AARCH64_HWCAP_LRCPC, 0)              \
  LINE(AARCH64_DCPOP, dcpop, "dcpop", AARCH64_HWCAP_DCPOP, 0)              \
  LINE(AARCH64_SHA3, sha3, "sha3", AARCH64_HWCAP_SHA3, 0)                  \
  LINE(AARCH64_SM3, sm3, "sm3", AARCH64_HWCAP_SM3, 0)                      \
  LINE(AARCH64_SM4, sm4, "sm4", AARCH64_HWCAP_SM4, 0)                      \
  LINE(AARCH64_ASIMDDP, asimddp, "asimddp", AARCH64_HWCAP_ASIMDDP, 0)      \
  LINE(AARCH64_SHA512, sha512, "sha512", AARCH64_HWCAP_SHA512, 0)          \
  LINE(AARCH64_SVE, sve, "sve", AARCH64_HWCAP_SVE, 0)                      \
  LINE(AARCH64_ASIMDFHM, asimdfhm, "asimdfhm", AARCH64_HWCAP_ASIMDFHM, 0)  \
  LINE(AARCH64_DIT, dit, "dit", AARCH64_HWCAP_DIT, 0)                      \
  LINE(AARCH64_USCAT, uscat, "uscat", AARCH64_HWCAP_USCAT, 0)              \
  LINE(AARCH64_ILRCPC, ilrcpc, "ilrcpc", AARCH64_HWCAP_ILRCPC, 0)          \
  LINE(AARCH64_FLAGM, flagm, "flagm", AARCH64_HWCAP_FLAGM, 0)              \
  LINE(AARCH64_SSBS, ssbs, "ssbs", AARCH64_HWCAP_SSBS, 0)                  \
  LINE(AARCH64_SB, sb, "sb", AARCH64_HWCAP_SB, 0)                          \
  LINE(AARCH64_PACA, paca, "paca", AARCH64_HWCAP_PACA, 0)                  \
  LINE(AARCH64_PACG, pacg, "pacg", AARCH64_HWCAP_PACG, 0)                  \
  LINE(AARCH64_DCPODP, dcpodp, "dcpodp", 0, AARCH64_HWCAP2_DCPODP)         \
  LINE(AARCH64_SVE2, sve2, "sve2", 0, AARCH64_HWCAP2_SVE2)                 \
  LINE(AARCH64_SVEAES, sveaes, "sveaes", 0, AARCH64_HWCAP2_SVEAES)         \
  LINE(AARCH64_SVEPMULL, svepmull, "svepmull", 0, AARCH64_HWCAP2_SVEPMULL) \
  LINE(AARCH64_SVEBITPERM, svebitperm, "svebitperm", 0,                    \
       AARCH64_HWCAP2_SVEBITPERM)                                          \
  LINE(AARCH64_SVESHA3, svesha3, "svesha3", 0, AARCH64_HWCAP2_SVESHA3)     \
  LINE(AARCH64_SVESM4, svesm4, "svesm4", 0, AARCH64_HWCAP2_SVESM4)         \
  LINE(AARCH64_FLAGM2, flagm2, "flagm2", 0, AARCH64_HWCAP2_FLAGM2)         \
  LINE(AARCH64_FRINT, frint, "frint", 0, AARCH64_HWCAP2_FRINT)             \
  LINE(AARCH64_SVEI8MM, svei8mm, "svei8mm", 0, AARCH64_HWCAP2_SVEI8MM)     \
  LINE(AARCH64_SVEF32MM, svef32mm, "svef32mm", 0, AARCH64_HWCAP2_SVEF32MM) \
  LINE(AARCH64_SVEF64MM, svef64mm, "svef64mm", 0, AARCH64_HWCAP2_SVEF64MM) \
  LINE(AARCH64_SVEBF16, svebf16, "svebf16", 0, AARCH64_HWCAP2_SVEBF16)     \
  LINE(AARCH64_I8MM, i8mm, "i8mm", 0, AARCH64_HWCAP2_I8MM)                 \
  LINE(AARCH64_BF16, bf16, "bf16", 0, AARCH64_HWCAP2_BF16)                 \
  LINE(AARCH64_DGH, dgh, "dgh", 0, AARCH64_HWCAP2_DGH)                     \
  LINE(AARCH64_RNG, rng, "rng", 0, AARCH64_HWCAP2_RNG)                     \
  LINE(AARCH64_BTI, bti, "bti", 0, AARCH64_HWCAP2_BTI)                     \
  LINE(AARCH64_MTE, mte, "mte", 0, AARCH64_HWCAP2_MTE)
#define INTROSPECTION_PREFIX Aarch64
#define INTROSPECTION_ENUM_PREFIX AARCH64

////////////////////////////////////////////////////////////////////////////////
// Implementation.
////////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>

#include "bit_utils.h"

#include <stdlib.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/machine.h>

#define MAXNAME 128

void DetectFeatures(int cpufamily, Aarch64Features* outFeatures)
{
	outFeatures->aes = true;
	outFeatures->sha1 = true;
	outFeatures->sha2 = true;
	outFeatures->pmull = true;
	outFeatures->crc32 = true;
	
	switch (cpufamily) {
			case CPUFAMILY_ARM_MONSOON_MISTRAL:
			case CPUFAMILY_ARM_VORTEX_TEMPEST:
			case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
				outFeatures->atomics = true;
			break;
		}
}

void DetectCpuName(int cpufamily, char* outName)
{
	size_t size = MAXNAME;
	char machineName[MAXNAME];
	sysctlbyname("hw.machine", machineName, &size, NULL, 0);
	
	uint32_t major = 0, minor = 0;
	sscanf(machineName, "%9[^,0123456789]%u,%u", outName, &major, &minor);

	uint32_t chipModel = 0;
	char suffix = '\0';

	if (strcmp(outName, "iPhone") == 0)
	{
		chipModel = major + 1;
	}
	else if (strcmp(outName, "iPad") == 0)
	{
		switch (major) {
			case 2:
				chipModel = major + 3;
			case 3:
				chipModel = (minor <= 3) ? 5 : 6;
				suffix = 'X';
				break;
			case 4:
				chipModel = major + 3;
				break;
			case 5:
				chipModel = major + 3;
				suffix = (minor <= 2) ? '\0' : 'X';
				break;
			case 6:
				chipModel = major + 3;
				suffix = minor <= 8 ? 'X' : '\0';
				break;
			case 7:
				chipModel = major + 3;
				suffix = minor <= 4 ? 'X' : '\0';
				break;
			default:
				strcpy(outName, "Unkown");
				break;
		}
	}
	else if (strcmp(outName, "iPod") == 0)
	{
		switch (major) {
			case 5:
				chipModel = 5;
				break;
			case 7:
				chipModel = 8;
				break;
			default:
				strcpy(outName, "Unkown");
				break;
		}
	}
	else if (strcmp(outName, "arm")==0)
	{
		size = MAXNAME;
		char modelName[MAXNAME];
		sysctlbyname("hw.model", modelName, &size, NULL, 0);
		strcpy(outName, modelName);
	}
	else
	{
		strcpy(outName, "Unkown device");
	}
	
	if (chipModel != 0) {
		sprintf(outName, "%s Apple A%u %c", outName, chipModel, suffix);
	}
}

bool GetAarch64Info(Aarch64Info* outInfo)
{
	size_t size;
	
	int type, subtype, cpufamily;
	
	sysctlbyname("hw.cputype", &type, &size, NULL, 0);
	sysctlbyname("hw.cpusubtype", &subtype, &size, NULL, 0);
	sysctlbyname("hw.cpufamily", &cpufamily, &size, NULL, 0);
	
	DetectFeatures(cpufamily, &outInfo->features);
	DetectCpuName(cpufamily, outInfo->name);
	
	return true;
}

#endif
