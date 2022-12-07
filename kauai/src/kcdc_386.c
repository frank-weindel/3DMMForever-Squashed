/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

/***************************************************************************
This c file is compiled to a command-line exe which when run produces
the 80386 asm version of the KCDC decompression code.

Register usage:
    eax: source bytes
    ebx: source address
    edi: destination address
    edx: working space
    ecx: for rep movsb
    esi: for rep movsb
***************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include "codkpri.h"

const int kcbitMinMoveByDword = 3;

const bool fFalse = 0;
const bool fTrue = 1;
static FILE *output = NULL;

void Setup(void);
void End(void);
void Advance(long cb);
void Test(long ibit);
void Block(long ibit);
void Literal(long ibit);
void Offset(long ibit, long cbit, long dibBase, long cbBase);
void GetLen(long ibit, bool fTestForDword);
void CallCopy(long ibit, long cbit, bool fTestForDword);
void Copy(long ibit, bool fDword);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <filename>", argv[0]);
        return EXIT_FAILURE;
    }
    output = fopen(argv[1], "w");
    if (output == NULL)
    {
        fprintf(stderr, "could not open %s", argv[1]);
        return EXIT_FAILURE;
    }

    long ibit;

    Setup();

    for (ibit = 0; ibit < 8; ibit++)
    {
        if (kcbitMinMoveByDword <= kcbitMaxLenKcdc)
        {
            Copy(ibit, fTrue);
            fprintf(output, "	jmp LBlock%d\n", ibit);
        }
        Copy(ibit, fFalse);
        Block(ibit);
    }

    End();
    return 0;
}

void Setup(void)
{
    fprintf(output, "	// Setup\n"
                    "	long cbTot;\n"
                    "	byte *pbLimDst = (byte *)pvDst + cbDst;\n\n"
                    "__asm\n"
                    "	{\n"
                    "	mov edi,pvDst\n"
                    "	mov ebx,pvSrc\n"
                    "	inc ebx\n");

    Advance(4);

    fprintf(output, "	jmp LBlock0\n");
}

void Copy(long ibit, bool fDword)
{
    if (fDword)
        fprintf(output, "\n	// Copy Dword %d\nLCopyDword%d:\n", ibit, ibit);
    else
        fprintf(output, "\n	// Copy Byte %d\nLCopyByte%d:\n", ibit, ibit);

    fprintf(output, "#ifdef SAFETY\n"
                    "	push edx\n"
                    "	lea edx,[edi+ecx]\n"
                    "	cmp edx,pbLimDst\n"
                    "	pop edx\n"
                    "	ja LFail\n"
                    "#endif //SAFETY\n");

    fprintf(output, "	neg esi\n"
                    "	add esi,edi\n");

    fprintf(output, "#ifdef SAFETY\n"
                    "	cmp esi,pvDst\n"
                    "	jb LFail\n"
                    "#endif //SAFETY\n");

    if (fDword)
    {
        fprintf(output, "	mov edx,ecx\n"
                        "	shr ecx,2\n"
                        "	and edx,3\n"
                        "	rep movsd\n"
                        "	mov ecx,edx\n"
                        "	rep movsb\n");
    }
    else
        fprintf(output, "	rep movsb\n");
}

void End(void)
{
    fprintf(output, "\nLDone:\n"
                    "	sub edi,pvDst\n"
                    "	mov cbTot,edi\n"
                    "	}\n\n");
}

void Advance(long cb)
{
    switch (cb)
    {
    case 0:
        break;
    case 1:
        fprintf(output, "	mov eax,[ebx-3]\n"
                        "	inc ebx\n");
        break;
    case 2:
        fprintf(output, "	mov eax,[ebx-2]\n"
                        "	add ebx,2\n");
        break;
    case 3:
        fprintf(output, "	mov eax,[ebx-1]\n"
                        "	add ebx,3\n");
        break;
    case 4:
        fprintf(output, "	mov eax,[ebx]\n"
                        "	add ebx,4\n");
        break;
    default:
        fprintf(output, "*** BUG\n");
        break;
    }
}

void Test(long ibit)
{
    if (ibit < 8)
        fprintf(output, "	test al,%d\n", 1 << ibit);
    else if (ibit < 16)
        fprintf(output, "	test ah,%d\n", 1 << (ibit - 8));
    else
        fprintf(output, "	test eax,%d\n", 1 << ibit);
}

void Block(long ibit)
{
    fprintf(output, "\n	// Block %d\n", ibit);
    fprintf(output, "LBlock%d:\n", ibit);

    Test(ibit);
    fprintf(output, "	jz LLiteral%d\n", ibit + 1);
    Test(ibit + 1);
    fprintf(output, "	jz L%dBit%d\n", kcbitKcdc0, ibit + 2);
    Test(ibit + 2);
    fprintf(output, "	jz L%dBit%d\n", kcbitKcdc1, ibit + 3);
    Test(ibit + 3);
    fprintf(output, "	jz L%dBit%d\n", kcbitKcdc2, ibit + 4);
    fprintf(output, "	jmp L%dBit%d\n", kcbitKcdc3, ibit + 4);

    Literal(ibit + 1);
    Offset(ibit + 2, kcbitKcdc0, kdibMinKcdc0, 2);
    Offset(ibit + 3, kcbitKcdc1, kdibMinKcdc1, 2);
    Offset(ibit + 4, kcbitKcdc2, kdibMinKcdc2, 2);
    Offset(ibit + 4, kcbitKcdc3, kdibMinKcdc3, 3);
}

void Literal(long ibit)
{
    fprintf(output, "\n	// Literal %d\n", ibit);
    fprintf(output, "LLiteral%d:\n", ibit);

    fprintf(output, "#ifdef SAFETY\n"
                    "	cmp edi,pbLimDst\n"
                    "	jae LFail\n"
                    "#endif //SAFETY\n");

    if (ibit == 8)
    {
        fprintf(output, "	mov [edi],ah\n"
                        "	inc edi\n");
    }
    else
    {
        fprintf(output,
                "	mov edx,eax\n"
                "	shr edx,%d\n"
                "	mov [edi],dl\n"
                "	inc edi\n",
                ibit);
    }

    ibit += 8;
    Advance(ibit / 8);
    fprintf(output, "	jmp LBlock%d\n", ibit & 0x07);
}

void Offset(long ibit, long cbit, long dibBase, long cbBase)
{
    fprintf(output, "\nL%dBit%d:\n", cbit, ibit);

    fprintf(output, "	mov esi,eax\n");
    fprintf(output, "	mov ecx,%d\n", cbBase);
    fprintf(output, "	shr esi,%d\n", ibit);
    fprintf(output, "	and esi,%d\n", (1 << cbit) - 1);

    if (kcbitKcdc3 == cbit)
    {
        // Put in the test for being done
        fprintf(output, "	cmp esi,%d\n", (1 << cbit) - 1);
        fprintf(output, "	je LDone\n");
    }

    if (1 == dibBase)
        fprintf(output, "	inc esi\n");
    else
        fprintf(output, "	add esi,%d\n", dibBase);

    ibit += cbit;
    Advance(ibit / 8);
    ibit &= 0x07;

    GetLen(ibit, dibBase < sizeof(long));
}

void GetLen(long ibit, bool fTestForDword)
{
    static long _cactCall = 0;
    long cbit;

    _cactCall++;
    for (cbit = 0; cbit <= kcbitMaxLenKcdc; cbit++)
    {
        Test(ibit + cbit);
        fprintf(output, "	jz LLen%d_%d\n", _cactCall, cbit);
    }
    fprintf(output, "	jmp LFail\n");

    for (cbit = 0; cbit <= kcbitMaxLenKcdc; cbit++)
    {
        fprintf(output, "LLen%d_%d:\n", _cactCall, cbit);
        if (cbit > 0)
        {
            fprintf(output, "	mov edx,eax\n");
            fprintf(output, "	shr edx,%d\n", ibit + cbit + 1);
            fprintf(output, "	add ecx,%d\n", (1 << cbit) - 1);
            fprintf(output, "	and edx,%d\n", (1 << cbit) - 1);
            fprintf(output, "	add ecx,edx\n");
        }
        CallCopy(ibit + cbit + cbit + 1, cbit, fTestForDword);
    }
}

void CallCopy(long ibit, long cbit, bool fTestForDword)
{
    Advance(ibit / 8);
    ibit &= 0x07;

    if (cbit < kcbitMinMoveByDword)
        fprintf(output, "jmp LCopyByte%d\n", ibit);
    else
    {
        if (fTestForDword)
        {
            fprintf(output, "	cmp esi,4\n");
            fprintf(output, "	jb LCopyByte%d\n", ibit);
        }
        fprintf(output, "	jmp LCopyDword%d\n", ibit);
    }
}
