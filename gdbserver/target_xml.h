#ifndef TARGET_XML_DEFINED
#define TARGET_XML_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

char* GetTargetXml(unsigned int Cookie_CPU, unsigned int Cookie_FPU, unsigned int* length);

#ifdef __cplusplus
}
#endif

#endif // TARGET_XML_DEFINED
