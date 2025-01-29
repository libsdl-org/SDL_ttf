static const unsigned char shader_frag_msl[] = {
  0x23, 0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x20, 0x3c, 0x6d, 0x65,
  0x74, 0x61, 0x6c, 0x5f, 0x73, 0x74, 0x64, 0x6c, 0x69, 0x62, 0x3e, 0x0a,
  0x23, 0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x20, 0x3c, 0x73, 0x69,
  0x6d, 0x64, 0x2f, 0x73, 0x69, 0x6d, 0x64, 0x2e, 0x68, 0x3e, 0x0a, 0x0a,
  0x75, 0x73, 0x69, 0x6e, 0x67, 0x20, 0x6e, 0x61, 0x6d, 0x65, 0x73, 0x70,
  0x61, 0x63, 0x65, 0x20, 0x6d, 0x65, 0x74, 0x61, 0x6c, 0x3b, 0x0a, 0x0a,
  0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x20, 0x6d, 0x61, 0x69, 0x6e, 0x30,
  0x5f, 0x6f, 0x75, 0x74, 0x0a, 0x7b, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x66,
  0x6c, 0x6f, 0x61, 0x74, 0x34, 0x20, 0x6f, 0x75, 0x74, 0x5f, 0x76, 0x61,
  0x72, 0x5f, 0x53, 0x56, 0x5f, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x20,
  0x5b, 0x5b, 0x63, 0x6f, 0x6c, 0x6f, 0x72, 0x28, 0x30, 0x29, 0x5d, 0x5d,
  0x3b, 0x0a, 0x7d, 0x3b, 0x0a, 0x0a, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74,
  0x20, 0x6d, 0x61, 0x69, 0x6e, 0x30, 0x5f, 0x69, 0x6e, 0x0a, 0x7b, 0x0a,
  0x20, 0x20, 0x20, 0x20, 0x66, 0x6c, 0x6f, 0x61, 0x74, 0x34, 0x20, 0x69,
  0x6e, 0x5f, 0x76, 0x61, 0x72, 0x5f, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f,
  0x52, 0x44, 0x30, 0x20, 0x5b, 0x5b, 0x75, 0x73, 0x65, 0x72, 0x28, 0x6c,
  0x6f, 0x63, 0x6e, 0x30, 0x29, 0x5d, 0x5d, 0x3b, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x66, 0x6c, 0x6f, 0x61, 0x74, 0x32, 0x20, 0x69, 0x6e, 0x5f, 0x76,
  0x61, 0x72, 0x5f, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x31,
  0x20, 0x5b, 0x5b, 0x75, 0x73, 0x65, 0x72, 0x28, 0x6c, 0x6f, 0x63, 0x6e,
  0x31, 0x29, 0x5d, 0x5d, 0x3b, 0x0a, 0x7d, 0x3b, 0x0a, 0x0a, 0x66, 0x72,
  0x61, 0x67, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x6d, 0x61, 0x69, 0x6e, 0x30,
  0x5f, 0x6f, 0x75, 0x74, 0x20, 0x6d, 0x61, 0x69, 0x6e, 0x30, 0x28, 0x6d,
  0x61, 0x69, 0x6e, 0x30, 0x5f, 0x69, 0x6e, 0x20, 0x69, 0x6e, 0x20, 0x5b,
  0x5b, 0x73, 0x74, 0x61, 0x67, 0x65, 0x5f, 0x69, 0x6e, 0x5d, 0x5d, 0x2c,
  0x20, 0x74, 0x65, 0x78, 0x74, 0x75, 0x72, 0x65, 0x32, 0x64, 0x3c, 0x66,
  0x6c, 0x6f, 0x61, 0x74, 0x3e, 0x20, 0x74, 0x65, 0x78, 0x20, 0x5b, 0x5b,
  0x74, 0x65, 0x78, 0x74, 0x75, 0x72, 0x65, 0x28, 0x30, 0x29, 0x5d, 0x5d,
  0x2c, 0x20, 0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x72, 0x20, 0x73, 0x61,
  0x6d, 0x70, 0x20, 0x5b, 0x5b, 0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x72,
  0x28, 0x30, 0x29, 0x5d, 0x5d, 0x29, 0x0a, 0x7b, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x6d, 0x61, 0x69, 0x6e, 0x30, 0x5f, 0x6f, 0x75, 0x74, 0x20, 0x6f,
  0x75, 0x74, 0x20, 0x3d, 0x20, 0x7b, 0x7d, 0x3b, 0x0a, 0x20, 0x20, 0x20,
  0x20, 0x6f, 0x75, 0x74, 0x2e, 0x6f, 0x75, 0x74, 0x5f, 0x76, 0x61, 0x72,
  0x5f, 0x53, 0x56, 0x5f, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x20, 0x3d,
  0x20, 0x69, 0x6e, 0x2e, 0x69, 0x6e, 0x5f, 0x76, 0x61, 0x72, 0x5f, 0x54,
  0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x30, 0x20, 0x2a, 0x20, 0x74,
  0x65, 0x78, 0x2e, 0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x28, 0x73, 0x61,
  0x6d, 0x70, 0x2c, 0x20, 0x69, 0x6e, 0x2e, 0x69, 0x6e, 0x5f, 0x76, 0x61,
  0x72, 0x5f, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x31, 0x29,
  0x3b, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e,
  0x20, 0x6f, 0x75, 0x74, 0x3b, 0x0a, 0x7d, 0x0a, 0x0a
};
static const unsigned int shader_frag_msl_len = 501;
