
#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>

namespace laspp {

#pragma pack(push, 1)
enum GlobalEncoding : uint16_t {
  GPS_TIME = 2 << 0,
  WAVEFORM_DATA_INTERNAL = 2 << 1,
  WAVEFORM_DATA_EXTERNAL = 2 << 2,
  SYNTHETIC_RETURN_NUMBERS = 2 << 3,
  WKT = 2 << 4,
};

inline std::string global_encoding_string(const uint16_t& encoding) {
  std::stringstream ss;
  if (encoding & GPS_TIME) ss << "GPS time, ";
  if (encoding & WAVEFORM_DATA_INTERNAL) ss << "Waveform data internal, ";
  if (encoding & WAVEFORM_DATA_EXTERNAL) ss << "Waveform data external, ";
  if (encoding & SYNTHETIC_RETURN_NUMBERS) ss << "Synthetic return numbers, ";
  if (encoding & WKT) ss << "WKT, ";
  return ss.str();
}

template <typename T, size_t N>
std::string arr_to_string(const T (&arr)[N]) {
  std::stringstream ss;
  ss << "[";
  for (size_t i = 0; i < N; ++i) {
    ss << arr[i];
    if (i < N - 1) ss << ", ";
  }
  ss << "]";
  return ss.str();
}

class __attribute__((packed)) LASHeader {
  char m_file_signature[4] = {'L', 'A', 'S', 'F'};
  uint16_t m_file_source_id;
  uint16_t m_global_encoding;
  uint32_t m_project_id_1;
  uint16_t m_project_id_2;
  uint16_t m_project_id_3;
  uint8_t m_project_id_4[8];
  uint8_t m_version_major;
  uint8_t m_version_minor;
  char m_system_id[32];
  char m_generating_software[32];
  uint16_t m_file_creation_day;
  uint16_t m_file_creation_year;
  uint16_t m_header_size;
  uint32_t m_offset_to_point_data;
  uint32_t m_number_of_variable_length_records;
  uint8_t m_point_data_record_format;
  uint16_t m_point_data_record_length;
  uint32_t m_legacy_number_of_point_records;
  uint32_t m_legacy_number_of_points_by_return[5];
  double m_x_scale_factor;
  double m_y_scale_factor;
  double m_z_scale_factor;
  double m_x_offset;
  double m_y_offset;
  double m_z_offset;
  double m_max_x;
  double m_min_x;  // Spec seems to disagree with this ordering :(
  double m_max_y;
  double m_min_y;
  double m_max_z;
  double m_min_z;
  uint64_t m_start_of_waveform_data_packet_record;
  uint64_t m_start_of_first_extended_variable_length_record;
  uint32_t m_number_of_extended_variable_length_records;
  uint64_t m_number_of_point_records;
  uint64_t m_number_of_points_by_return[15];

 public:
  static LASHeader FromFile(std::ifstream& file) {
    LASHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(LASHeader));
    return header;
  }

  bool is_laz_compressed() const {
    return m_point_data_record_format &
           128;  // The spec says laz adds 100 but other implementations use 128
  }

  size_t num_points() const {
    return m_legacy_number_of_point_records == 0 ? m_number_of_point_records
                                                 : m_legacy_number_of_point_records;
  }

  unsigned int point_data_record_length() const { return m_point_data_record_length; }

  unsigned int offset_to_point_data() const { return m_offset_to_point_data; }

  unsigned int VLR_offset() const { return m_header_size; }
  unsigned int VLR_count() const { return m_number_of_variable_length_records; }

  size_t EVLR_offset() const {
    return offsetof(LASHeader, m_start_of_first_extended_variable_length_record) < m_header_size
               ? m_start_of_first_extended_variable_length_record
               : 0;
  }
  size_t EVLR_count() const {
    return offsetof(LASHeader, m_start_of_first_extended_variable_length_record) < m_header_size
               ? m_number_of_extended_variable_length_records
               : 0;
  }

  friend std::ostream& operator<<(std::ostream& os, const LASHeader& header) {
    os << "File signature: " << header.m_file_signature[0] << header.m_file_signature[1]
       << header.m_file_signature[2] << header.m_file_signature[3] << std::endl;
    os << "File source ID: " << header.m_file_source_id << std::endl;
    os << "Global encoding: " << global_encoding_string(header.m_global_encoding) << std::endl;
    os << "Project ID 1: " << header.m_project_id_1 << std::endl;
    os << "Project ID 2: " << header.m_project_id_2 << std::endl;
    os << "Project ID 3: " << header.m_project_id_3 << std::endl;
    os << "Project ID 4: " << arr_to_string(header.m_project_id_4) << std::endl;
    os << "Version major: " << header.m_version_major << std::endl;
    os << "Version minor: " << header.m_version_minor << std::endl;
    os << "System ID: " << header.m_system_id << std::endl;
    os << "Generating software: " << header.m_generating_software << std::endl;
    os << "File creation day: " << header.m_file_creation_day << std::endl;
    os << "File creation year: " << header.m_file_creation_year << std::endl;
    os << "Header size: " << header.m_header_size << std::endl;
    os << "Offset to point data: " << header.m_offset_to_point_data << std::endl;
    os << "Number of variable length records: " << header.m_number_of_variable_length_records
       << std::endl;
    os << "Point data record format: " << (uint)header.m_point_data_record_format << std::endl;
    os << "Point data record length: " << header.m_point_data_record_length << std::endl;
    os << "Legacy number of point records: " << header.m_legacy_number_of_point_records
       << std::endl;
    os << "Legacy number of points by return: "
       << arr_to_string(header.m_legacy_number_of_points_by_return) << std::endl;
    os << "X scale factor: " << header.m_x_scale_factor << std::endl;
    os << "Y scale factor: " << header.m_y_scale_factor << std::endl;
    os << "Z scale factor: " << header.m_z_scale_factor << std::endl;
    os << "X offset: " << header.m_x_offset << std::endl;
    os << "Y offset: " << header.m_y_offset << std::endl;
    os << "Z offset: " << header.m_z_offset << std::endl;
    os << "Max X: " << header.m_max_x << std::endl;
    os << "Max Y: " << header.m_max_y << std::endl;
    os << "Max Z: " << header.m_max_z << std::endl;
    os << "Min X: " << header.m_min_x << std::endl;
    os << "Min Y: " << header.m_min_y << std::endl;
    os << "Min Z: " << header.m_min_z << std::endl;
    os << "Start of waveform data packet record: " << header.m_start_of_waveform_data_packet_record
       << std::endl;
    os << "Start of first extended variable length record: "
       << header.m_start_of_first_extended_variable_length_record << std::endl;
    os << "Number of extended variable length records: "
       << header.m_number_of_extended_variable_length_records << std::endl;
    os << "Number of point records: " << header.m_number_of_point_records << std::endl;
    os << "Number of points by return: " << arr_to_string(header.m_number_of_points_by_return)
       << std::endl;
    return os;
  }
};

#pragma pack(pop)

}  // namespace laspp
