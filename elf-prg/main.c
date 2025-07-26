#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#define bool short
#define true 1
#define false 0

unsigned char ReadUInt8(FILE *fs, bool* status)
{
	unsigned char v;
	*status = fread(&v, 1,1, fs) == 1;
	return v;
}

unsigned short ReadUInt16_r(FILE *fs, bool* status)
{
	size_t cnt = 0;
	unsigned short v;
	unsigned char* vp = (unsigned char*)&v;
	cnt += fread(vp + 1, 1,1, fs);
	cnt += fread(vp, 1,1, fs);
	*status &= cnt == 2;
	return v;
}

unsigned int ReadUInt32_r(FILE *fs, bool* status)
{
	size_t cnt = 0;
	unsigned int v;
	unsigned char* vp = (unsigned char*)&v;
	cnt += fread(vp + 3, 1,1, fs);
	cnt += fread(vp + 2, 1,1, fs);
	cnt += fread(vp + 1, 1,1, fs);
	cnt += fread(vp, 1,1, fs);
	*status &= cnt == 4;
	return v;
}

// Don't forget to free.
void* ReadSectionData(FILE *fs, Elf32_Shdr* sectionHeaders, size_t sectionIndex)
{
	size_t offset = sectionHeaders[sectionIndex].sh_offset;
	size_t size = sectionHeaders[sectionIndex].sh_size;
//	printf("section: %ld, %ld, %ld\n",offset, size, sectionIndex);
	fseek(fs, offset, SEEK_SET);
	void* data = NULL;
	if ((data = malloc(size)) != NULL)
	{
		if (fread(data, 1, size, fs) != size)
		{
			free(data);
			data = NULL;
		}
	}
	return data;
}

bool ReadHeader(FILE* fs, Elf32_Ehdr* header)
{
	bool status = true;
	do
	{
		status &= EI_NIDENT == fread(header->e_ident, 1, EI_NIDENT, fs);
		if (!status) {break;}
		char m68kelf_magic[EI_NIDENT] = { 0x7f, 'E', 'L', 'F', 0x01, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
		status &= 0 == memcmp(header->e_ident, m68kelf_magic, EI_NIDENT);
		if (!status) {break;}
		header->e_type = ReadUInt16_r(fs, &status);
		header->e_machine = ReadUInt16_r(fs, &status);
		header->e_version = ReadUInt32_r(fs, &status);
		header->e_entry = ReadUInt32_r(fs, &status);
		header->e_phoff = ReadUInt32_r(fs, &status);
		header->e_shoff = ReadUInt32_r(fs, &status);
		header->e_flags = ReadUInt32_r(fs, &status);
		header->e_ehsize = ReadUInt16_r(fs, &status);
		header->e_phentsize = ReadUInt16_r(fs, &status);
		header->e_phnum = ReadUInt16_r(fs, &status);
		header->e_shentsize = ReadUInt16_r(fs, &status);
		header->e_shnum = ReadUInt16_r(fs, &status);
		header->e_shstrndx = ReadUInt16_r(fs, &status);
		if (!status) {break;}
		status &= header->e_version == 1 && header->e_machine == 4;
		status &= header->e_ehsize == 0x34 && (header->e_phentsize == 0x20 || header->e_phentsize == 0) && header->e_shentsize == 0x28;
	} while(false);
	return status;
}

		
Elf32_Shdr* ReadSectionHeaders(FILE* fs, Elf32_Ehdr* header)
{
	Elf32_Shdr* sectionHeaders = malloc(header->e_shnum * sizeof(Elf32_Shdr));
	fseek(fs, header->e_shoff, SEEK_SET);
	for (size_t i = 0; i < header->e_shnum; ++i)
	{
		bool status = true;
		Elf32_Shdr* sh = sectionHeaders + i;
		sh->sh_name = ReadUInt32_r(fs, &status);
		sh->sh_type = ReadUInt32_r(fs, &status);
		sh->sh_flags = ReadUInt32_r(fs, &status);
		sh->sh_addr = ReadUInt32_r(fs, &status);
		sh->sh_offset = ReadUInt32_r(fs, &status);
		sh->sh_size = ReadUInt32_r(fs, &status);
		sh->sh_link = ReadUInt32_r(fs, &status);
		sh->sh_info = ReadUInt32_r(fs, &status);
		sh->sh_addralign = ReadUInt32_r(fs, &status);
		sh->sh_entsize = ReadUInt32_r(fs, &status);
		if (!status) 
		{
			free(sectionHeaders);
			sectionHeaders = NULL;
			break;
		}
	}
	return sectionHeaders;
}

int FindTypedSection(Elf32_Ehdr* header, Elf32_Shdr* sectionHeaders, Elf32_Word findType)
{
	for (int i = 0; i < header->e_shnum; ++i)
	{
		if (sectionHeaders[i].sh_type == findType)
		{
			return i;
		}
	}	
	return -1;
}

int FindNamedSection(Elf32_Ehdr* header, Elf32_Shdr* sectionHeaders, const char* sectionHeaderStrings, const char* name)
{
	for (int i = 0; i < header->e_shnum; ++i)
	{
		const char* sectionName = sectionHeaderStrings + sectionHeaders[i].sh_name;
		if (strcmp(name, sectionName) == 0)
		{
			return i;
		}
	}	
	return -1;
}
		
bool OutputSectionProgbits(FILE* fs, FILE* fd, Elf32_Shdr* sectionHeader)
{
	bool status = false;
	void* data = NULL;
	// Section .text and .data contains progbits that we want.
	if (sectionHeader->sh_type == SHT_PROGBITS)
	{
		if ((data = malloc(sectionHeader->sh_size)) != NULL)
		{
			fseek(fs, sectionHeader->sh_offset, SEEK_SET);
			if (fread(data, 1, sectionHeader->sh_size, fs) == sectionHeader->sh_size)
			{
				if (fwrite(data, 1, sectionHeader->sh_size, fd) == sectionHeader->sh_size)
				{
					// success
					status = true;
				}
			}
		}
	}
	if (data != NULL)
	{
		free(data);
		data = NULL;
	}
	return status;
}

// Just makes sure that the alignment of the section to be written, matches the position in the written file.
bool AlignToSectionAddress(FILE* fd, Elf32_Shdr* sectionHeader)
{
	int align = sectionHeader->sh_addr - (ftell(fd) - 0x1c);
	if (align < 0)
	{
		// Cannot align backwards!
		return false;
	}
	for (;align != 0; --align)
	{
		char zero = 0;
		fwrite(&zero, 1, 1, fd);
	}
	return true;
}

bool LoadSymbols(FILE* fs, Elf32_Shdr* sectionHeader, Elf32_Sym** pt_symbols)
{
	bool status = true;
	if (sectionHeader->sh_type == SHT_SYMTAB)
	{
		fseek(fs, sectionHeader->sh_offset, SEEK_SET);
		
		size_t numbytes = sectionHeader->sh_size;
		size_t numsyms = numbytes / sizeof(Elf32_Sym);
		Elf32_Sym* symbols = (Elf32_Sym*)malloc(numbytes);
		*pt_symbols = symbols;
		if (symbols == NULL)
		{
			return false;
		}
		for (size_t i = 0; i < numsyms; ++i)
		{			
			symbols[i].st_name = ReadUInt32_r(fs, &status);
			symbols[i].st_value = ReadUInt32_r(fs, &status);
			symbols[i].st_size = ReadUInt32_r(fs, &status);
			symbols[i].st_info = ReadUInt8(fs, &status);
			symbols[i].st_other = ReadUInt8(fs, &status);
			symbols[i].st_shndx = ReadUInt16_r(fs, &status);
			if (!status) {break;}
		}
	}
	return status;
}

bool LoadRelocs(FILE* fs, Elf32_Shdr* sectionHeader, Elf32_Sym* pt_symbols, const char* symbolTableStrings, Elf32_Rela** pt_relocs, size_t* relocnum)
{
	bool status = true;
	// Section .rela.text and .rela.data contains relocation data that we want.
	if (sectionHeader->sh_type == SHT_RELA)
	{
		fseek(fs, sectionHeader->sh_offset, SEEK_SET);
		
		size_t addnumbytes = sectionHeader->sh_size;
		size_t addnumrelocs = addnumbytes / sizeof(Elf32_Rela);
		size_t lastrelocnum = *relocnum;
		size_t totbytes = (lastrelocnum * sizeof(Elf32_Rela)) + addnumbytes;

		Elf32_Rela* relocs = *pt_relocs;
		if (relocs == NULL)
		{
			relocs = (Elf32_Rela*)malloc(totbytes);
		}
		else
		{
			relocs = (Elf32_Rela*)realloc(relocs, totbytes);
		}
		*pt_relocs = relocs;
		if (relocs == NULL)
		{
			return false;
		}
		
		// Add the new relocs and discard all weak links that are undefined.
		size_t j = lastrelocnum;
		for (size_t i = 0; i < addnumrelocs; ++i)
		{		
			relocs[j].r_offset = ReadUInt32_r(fs, &status);
			relocs[j].r_info = ReadUInt32_r(fs, &status);
			relocs[j].r_addend = ReadUInt32_r(fs, &status);
			if (!status) {break;}
			
			Elf32_Word symbol = ELF32_R_SYM(relocs[j].r_info);
			Elf32_Sym* pt_sym = &pt_symbols[symbol];
			unsigned char symbol_bind = ELF32_ST_BIND(pt_sym->st_info);
			Elf32_Half symbol_section = pt_sym->st_shndx;
			if (symbol_bind == STB_WEAK && symbol_section == SHN_UNDEF)
			{
			    // The relocation points to a symbol that is an unassigned weak link.
			    // We should not relocate that as it must remain a zero.
			    if (symbolTableStrings != NULL)
			    {
			        const char* name = symbolTableStrings + pt_sym->st_name;
			        printf ("Skipped *UND* weak symbol: %s\n", name);
			    }
			}
			else
			{
			    ++j;
			}
		}
		*relocnum = j;
	}
	return status;
}

void SortRelocs(Elf32_Rela* relocs, size_t relocnum)
{
	Elf32_Rela t;
	if (relocnum == 0) { return; }
	// Just a simple bubble sort.
	for (size_t i = 0; i < relocnum - 1; ++i)
	{
		for (size_t j = i + 1; j < relocnum; ++j)
		{
			if (relocs[i].r_offset > relocs[j].r_offset)
			{
				// flip it
				t = relocs[i];
				relocs[i] = relocs[j];
				relocs[j] = t;
			}
		}
	}
}

bool WriteFixup(FILE* fd, Elf32_Rela* relocs, size_t relocnum)
{
	bool status = true;
	unsigned int lastOffset = 0;
	
	for (size_t i = 0; i < relocnum && status; ++i)
	{
		unsigned short type = ELF32_R_TYPE(relocs[i].r_info);
		if (type == R_68K_32)	// The only type we relocate
		{
			// As we are assuming a use of atari-st.ld, then we can also assume
			// that r_offset points to the correct place in the prg file.
			if (lastOffset == 0)
			{
				char* v = (char*)&(relocs[i].r_offset);
				status &= fwrite(v + 3, 1, 1, fd) == 1;
				status &= fwrite(v + 2, 1, 1, fd) == 1;
				status &= fwrite(v + 1, 1, 1, fd) == 1;
				status &= fwrite(v + 0, 1, 1, fd) == 1;
			}
			else
			{
				unsigned int deltaFix = relocs[i].r_offset - lastOffset;
				while (deltaFix > 254)
				{
					char one = 1;
					status &= fwrite(&one, 1, 1, fd) == 1;
					deltaFix -= 254;
				}
				status &= fwrite(&deltaFix, 1, 1, fd) == 1;
			}
			lastOffset = relocs[i].r_offset;
		}
		else if (type == R_68K_NONE)
		{
			// No relocation
		}
		else if (type == R_68K_PC8 || type == R_68K_PC16 || type == R_68K_PC32)
		{
			// pc relative do not need relocation.
		}
		else
		{
			// Not a type we know how to handle!
			printf("Unknown type of relocation data: %x at address: %x\n", type, relocs[i].r_offset);
			status = false;
		}
	}
	
	// end fixup table.
	if (lastOffset == 0)
	{
		status &= fwrite(&lastOffset, 1, 4, fd) == 4;
	}
	else
	{
		// 1 byte zero to end a fixup table.
		unsigned char justZero = 0;
		status &= fwrite(&justZero, 1, 1, fd) == 1;
	}
	return status;
}

int main(int argc, char *argv[])
{
	printf("Elf to prg conversion...\n");
	int result = -1;
	if (argc != 3)
	{
		printf("Not correct number of arguments!\n");
		return -1;
	}
	char* source = argv[1];
	char* destination = argv[2];
	
	FILE* fs = NULL;
	FILE* fd = NULL;
	Elf32_Shdr*	sectionHeaders = NULL;
	char* sectionHeaderStrings = NULL;
	char* symbolTableStrings = NULL;
	Elf32_Sym* symbols = NULL;
	Elf32_Rela* relocs = NULL;
	do
	{
		if ((fs = fopen(source, "rb")) == NULL)
		{
			printf("Could not open %s\n", source);
			break;
		}		
		if ((fd = fopen(destination, "wb")) == NULL)
		{
			printf("Could not open %s\n", destination);
			break;
		}	
		// Read and verify header.
		Elf32_Ehdr header;
		if (!ReadHeader(fs, &header))
		{
			printf("Could not read elf header\n");
			break;
		}		
		
		// Read section headers
		if (NULL == (sectionHeaders = ReadSectionHeaders(fs, &header)))
		{
			printf("Could not read elf section headers\n");
			break;
		}		
		
		// Read section header strings.
		if (NULL == (sectionHeaderStrings = ReadSectionData(fs, sectionHeaders, header.e_shstrndx)))
		{
			printf("Could not read section header strings\n");
			break;
		}
		
		// Read symbol table strings.
		int stringHeaderIndex = FindTypedSection(&header, sectionHeaders, SHT_STRTAB);
		if (stringHeaderIndex >= 0)
		{
	                if (NULL == (symbolTableStrings = ReadSectionData(fs, sectionHeaders, stringHeaderIndex)))
	                {
		                printf("Could not read symbol table strings\n");
		                break;
	                }
		}

		int prgHeaderIndex = FindNamedSection(&header, sectionHeaders, sectionHeaderStrings, ".prgheader");
		if (prgHeaderIndex < 0)
		{
			printf("Missing .header section\n");
			break;
		}
		int textIndex = FindNamedSection(&header, sectionHeaders, sectionHeaderStrings, ".text");
		if (textIndex < 0)
		{
			printf("Missing .text section\n");
			break;
		}
		int relaTextIndex = FindNamedSection(&header, sectionHeaders, sectionHeaderStrings, ".rela.text");
		if (relaTextIndex < 0)
		{
			printf("Missing .rela.text section\n");
			// Not an error, the section can be missing if nothing needs to be relocated.
			//break;	
		}
		int dataIndex = FindNamedSection(&header, sectionHeaders, sectionHeaderStrings, ".data");
		if (dataIndex < 0)
		{
			printf("Missing .data section\n");
			break;
		}
		int relaDataIndex = FindNamedSection(&header, sectionHeaders, sectionHeaderStrings, ".rela.data");
		if (relaDataIndex < 0)
		{
			// Not an error, the section can be missing if nothing needs to be relocated.
			printf("Missing .rela.data section\n");
			//break;
		}
		int bssIndex = FindNamedSection(&header, sectionHeaders, sectionHeaderStrings, ".bss");
		if (bssIndex < 0)
		{
			printf("Missing .bss section\n");
			break;
		}
		int symIndex = FindNamedSection(&header, sectionHeaders, sectionHeaderStrings, ".symtab");
		if (symIndex < 0)
		{
			printf("Missing .symtab section\n");
			break;
		}
		
		// When outputting the prg, we assume that the elf have been linked with atari-st.ld
		if (!OutputSectionProgbits(fs, fd, sectionHeaders + prgHeaderIndex))
		{
			printf("Could not handle section .header\n");
			break;
		}
		if (!OutputSectionProgbits(fs, fd, sectionHeaders + textIndex))
		{
			printf("Could not handle section .text\n");
			break;
		}		
		if (!AlignToSectionAddress(fd, sectionHeaders + dataIndex))
		{
			printf("Could not align section .data\n");
			break;
		}
		if (!OutputSectionProgbits(fs, fd, sectionHeaders + dataIndex))
		{
			printf("Could not handle section .data\n");
			break;
		}
		if (!AlignToSectionAddress(fd, sectionHeaders + bssIndex))
		{
			printf("Could not align section .bss\n");
			break;
		}
		
		// BSS section was fixed during linking, so just ignore it.
		
		// Load all symbols
		if (symIndex >= 0)
		{
			if (!LoadSymbols(fs, sectionHeaders + symIndex, &symbols))
			{
				printf("Could not load section .symtab\n");
				break;
			}	
		}
		
		// Load all relocations
		size_t relocnum = 0;
		if (relaTextIndex >= 0)
		{
			if (!LoadRelocs(fs, sectionHeaders + relaTextIndex, symbols, symbolTableStrings, &relocs, &relocnum))
			{
				printf("Could not load section .rela.text\n");
				break;
			}	
		}		
		if (relaDataIndex >= 0)
		{
			if (!LoadRelocs(fs, sectionHeaders + relaDataIndex, symbols, symbolTableStrings, &relocs, &relocnum))
			{
				printf("Could not load section .rela.data\n");
				break;
			}			
		}		
		// Sort relocations
		SortRelocs(relocs, relocnum);
		
		if (!WriteFixup(fd, relocs, relocnum))
		{
			printf("Could not write fixup data\n");
			break;
		}			

		// And we are done!
	        printf("Elf to prg conversion done.\n");
		result = 0;
	} while (false);

	if (relocs != NULL)
	{
		free(relocs);
		relocs = NULL;
	}
	if (sectionHeaderStrings != NULL)
	{
		free(sectionHeaderStrings);
		sectionHeaderStrings = NULL;
	}
	if (symbolTableStrings != NULL)
	{
		free(symbolTableStrings);
		symbolTableStrings = NULL;
	}
	if (sectionHeaders != NULL)
	{
		free(sectionHeaders);
		sectionHeaders = NULL;
	}
	if (fs != NULL)
	{
		fclose(fs);
		fs = NULL;
	}		
	if (fd != NULL)
	{
		fclose(fd);
		fd = NULL;
	}		
	return result;
}
