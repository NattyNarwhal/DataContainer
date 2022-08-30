#include "parsing.hpp"

char const * advance_to_closing_bracket(char const * pos, char const * end) {
	int32_t depth_count = 0;
	while(pos < end) {
		if(*pos == '{')
			++depth_count;
		else if(*pos == '}')
			--depth_count;

		if(depth_count < 0)
			return pos;
		++pos;
	}
	return pos;
}

char const * advance_to_non_whitespace(char const * pos, char const * end) {
	while(pos < end) {
		if(*pos != ' ' && *pos != '\t' && *pos != '\n' && *pos != '\r')
			return pos;
		++pos;
	}
	return pos;
}

char const * advance_to_whitespace(char const * pos, char const * end) {
	while(pos < end) {
		if(*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')
			return pos;
		++pos;
	}
	return pos;
}

char const * advance_to_whitespace_or_brace(char const * pos, char const * end) {
	while(pos < end) {
		if(*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r' || *pos == '{')
			return pos;
		++pos;
	}
	return pos;
}

char const * reverse_to_non_whitespace(char const * start, char const * pos) {
	while(pos > start) {
		if(*(pos - 1) != ' ' && *(pos - 1) != '\t' && *(pos - 1) != '\n' && *(pos - 1) != '\r')
			return pos;
		--pos;
	}
	return pos;
}

int32_t calculate_line_from_position(char const * start, char const * pos) {
	int32_t line_count = 1;

	char const* t = start;
	while(t < pos) {
		if(*t == '\n')
			++line_count;
		++t;
	}

	return line_count;
}

parsed_item extract_item(char const * input, char const * end, char const * global_start, error_record & err) {
	parsed_item result;

	result.key.start = advance_to_non_whitespace(input, end);
	result.key.end = advance_to_whitespace_or_brace(result.key.start, end);

	char const* position = advance_to_non_whitespace(result.key.end, end);
	result.terminal = position;

	while(position < end) {
		if(*position != '{')
			break;
		char const* value_close = advance_to_closing_bracket(position + 1, end);
		if(value_close == end) {
			err.add(std::string("Open bracket on line ") + std::to_string(calculate_line_from_position(global_start, position)) + " was not properly closed.");
		}
		result.values.push_back(char_range{ position + 1, reverse_to_non_whitespace(position + 1, value_close) });
		position = advance_to_non_whitespace(value_close + 1, end);
		result.terminal = position;
	}

	return result;
}

load_save_def parse_load_save_def(char const* start, char const* end, char const* global_start, error_record& err_out) {
	load_save_def result;
	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "name") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"name\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.name = extracted.values[0].to_string();
				}
			} else if(kstr == "only_objects") {
				if(extracted.values.size() == 0) {
					err_out.add(std::string("wrong number of parameters for \"only_objects\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(result.objects_filter != filter_type::default_exclude) {
					result.objects_filter = filter_type::include;
					for(uint32_t i = 0; i < extracted.values.size(); ++ i)
						result.obj_tags.push_back(extracted.values[0].to_string());
				} else {
					err_out.add(std::string("illegal setting of the object filter a second time on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else if(kstr == "exclude_objects") {
				if(extracted.values.size() == 0) {
					err_out.add(std::string("wrong number of parameters for \"exclude_objects\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(result.objects_filter != filter_type::default_exclude) {
					result.objects_filter = filter_type::exclude;
					for(uint32_t i = 0; i < extracted.values.size(); ++i)
						result.obj_tags.push_back(extracted.values[0].to_string());
				} else {
					err_out.add(std::string("illegal setting of the object filter a second time on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else if(kstr == "only_properties") {
				if(extracted.values.size() == 0) {
					err_out.add(std::string("wrong number of parameters for \"only_properties\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(result.objects_filter != filter_type::default_exclude) {
					result.properties_filter = filter_type::include;
					for(uint32_t i = 0; i < extracted.values.size(); ++i)
						result.property_tags.push_back(extracted.values[0].to_string());
				} else {
					err_out.add(std::string("illegal setting of the properties filter a second time on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else if(kstr == "exclude_properties") {
				if(extracted.values.size() == 0) {
					err_out.add(std::string("wrong number of parameters for \"exclude_properties\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(result.objects_filter != filter_type::default_exclude) {
					result.properties_filter = filter_type::exclude;
					for(uint32_t i = 0; i < extracted.values.size(); ++i)
						result.property_tags.push_back(extracted.values[0].to_string());
				} else {
					err_out.add(std::string("illegal setting of the properties filter a second time on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing loading/saving routine defintion on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

related_object parse_link_def(char const * start, char const * end, char const * global_start, error_record & err_out) {
	related_object result;

	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "name") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"name\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.property_name = extracted.values[0].to_string();
				}
			} else if(kstr == "type") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"type\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(extracted.values[0].to_string() == "unique") {
					result.index = index_type::at_most_one;
				} else if(extracted.values[0].to_string() == "many") {
					result.index = index_type::many;
				} else if(extracted.values[0].to_string() == "unindexed") {
					result.index = index_type::none;
				} else {
					err_out.add(std::string("unknown parameter \"") + extracted.values[0].to_string() + "\" passed to type on line "
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else if(kstr == "object") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"object\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.type_name = extracted.values[0].to_string();
				}
			} else if(kstr == "index_storage") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"index_storage\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(extracted.values[0].to_string() == "std_vector") {
					result.ltype = list_type::std_vector;
				} else if(extracted.values[0].to_string() == "list") {
					result.ltype = list_type::list;
				} else if(extracted.values[0].to_string() == "array") {
					result.ltype = list_type::array;
				} else {
					err_out.add(std::string("unknown parameter \"") + extracted.values[0].to_string() + "\" passed to index_storage on line "
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing link defintion on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

composite_index_def parse_composite_key(char const * start, char const * end, char const * global_start, error_record & err_out) {
	composite_index_def result;

	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "name") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"name\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.name = extracted.values[0].to_string();
				}
			} else if(kstr == "index") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"index\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.component_indexes.push_back(extracted.values[0].to_string());
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing composite key on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

property_def parse_property_def(char const * start, char const * end, char const * global_start, error_record & err_out) {
	property_def result;

	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "name") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"name\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.name = extracted.values[0].to_string();
				}
			} else if(kstr == "type") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"type\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					auto inner_extracted = extract_item(extracted.values[0].start, extracted.values[0].end, start, err_out);
					std::string ikstr = inner_extracted.key.to_string();

					if(ikstr == "bitfield") {
						result.type = property_type::bitfield;
					} else if(ikstr == "object") {
						if(inner_extracted.values.size() != 1) {
							err_out.add(std::string("wrong number of parameters for \"object\" on line ")
								+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
						} else {
							result.type = property_type::object;
							result.data_type = inner_extracted.values[0].to_string();
						}
					} else if(ikstr == "derived") {
						if(inner_extracted.values.size() != 1) {
							err_out.add(std::string("wrong number of parameters for \"derived\" on line ")
								+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
						} else {
							result.is_derived = true;
							result.type = property_type::other;
							result.data_type = inner_extracted.values[0].to_string();
						}
					} else if(ikstr == "vector_pool") {
						if(inner_extracted.values.size() != 2) {
							err_out.add(std::string("wrong number of parameters for \"vector_pool\" on line ")
								+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
						} else {
							result.type = property_type::special_vector;
							result.special_pool_size = std::stoi(inner_extracted.values[0].to_string());
							result.data_type = inner_extracted.values[1].to_string();
						}
					} else {
						if(inner_extracted.values.size() != 0) {
							err_out.add(std::string("unexpected key \"" + ikstr + "\" on line ")
								+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
						} else {
							result.type = property_type::other;
							result.data_type = inner_extracted.key.to_string();
						}
					}
				}
			} else if(kstr == "hook") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"hook\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(extracted.values[0].to_string() == "get") {
					result.hook_get = true;
				} else if(extracted.values[0].to_string() == "set") {
					result.hook_set = true;
				} else {
					err_out.add(std::string("unknown parameter \"") + extracted.values[0].to_string() + "\" passed to hook on line "
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else if(kstr == "tag") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"tag\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.property_tags.push_back(extracted.values[0].to_string());
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing property defintion on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

relationship_object_def parse_relationship(char const * start, char const * end, char const * global_start, error_record & err_out) {
	relationship_object_def result;

	result.is_relationship = true;

	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "name") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"name\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.name = extracted.values[0].to_string();
				}
			} else if(kstr == "primary_key") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"primary_key\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.force_pk = extracted.values[0].to_string();
				}
			} else if(kstr == "storage_type") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"storage_type\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					if(extracted.values[0].to_string() == "contiguous") {
						result.store_type = storage_type::contiguous;
					} else if(extracted.values[0].to_string() == "erasable") {
						result.store_type = storage_type::erasable;
					} else if(extracted.values[0].to_string() == "compactable") {
						result.store_type = storage_type::compactable;
					} else {
						err_out.add(std::string("unknown parameter \"") + kstr + "\" passed to storage_type on line "
							+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
					}
				}
			} else if(kstr == "size") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"size\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					if(extracted.values[0].to_string() == "expandable") {
						result.is_expandable = true;
						result.size = 0;
					} else {
						result.is_expandable = false;
						result.size = std::stoi(extracted.values[0].to_string());
					}
				}
			} else if(kstr == "tag") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"tag\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.obj_tags.push_back(extracted.values[0].to_string());
				}
			} else if(kstr == "property") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"property\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.properties.push_back(
						parse_property_def(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			} else if(kstr == "link") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"link\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.indexed_objects.push_back(
						parse_link_def(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			} else if(kstr == "composite_key") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"composite_key\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.composite_indexes.push_back(
						parse_composite_key(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			} else if(kstr == "hook") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"hook\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(extracted.values[0].to_string() == "create") {
					result.hook_create = true;
				} else if(extracted.values[0].to_string() == "delete") {
					result.hook_delete = true;
				} else if(extracted.values[0].to_string() == "move") {
					result.hook_delete = true;
				} else {
					err_out.add(std::string("unknown parameter \"") + extracted.values[0].to_string() + "\" passed to hook on line "
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing relationship defintion on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

relationship_object_def parse_object(char const * start, char const * end, char const * global_start, error_record & err_out) {
	relationship_object_def result;

	result.is_relationship = false;

	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "name") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"name\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.name = extracted.values[0].to_string();
				}
			} else if(kstr == "storage_type") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"storage_type\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					if(extracted.values[0].to_string() == "contiguous") {
						result.store_type = storage_type::contiguous;
					} else if(extracted.values[0].to_string() == "erasable") {
						result.store_type = storage_type::erasable;
					} else if(extracted.values[0].to_string() == "compactable") {
						result.store_type = storage_type::compactable;
					} else {
						err_out.add(std::string("unknown parameter \"") + kstr + "\" passed to storage_type on line "
							+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
					}
				}
			} else if(kstr == "size") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"size\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					if(extracted.values[0].to_string() == "expandable") {
						result.is_expandable = true;
						result.size = 0;
					} else {
						result.is_expandable = false;
						result.size = std::stoi(extracted.values[0].to_string());
					}
				}
			} else if(kstr == "tag") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"tag\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.obj_tags.push_back(extracted.values[0].to_string());
				}
			} else if(kstr == "hook") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"hook\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(extracted.values[0].to_string() == "create") {
					result.hook_create = true;
				} else if(extracted.values[0].to_string() == "delete") {
					result.hook_delete = true;
				} else if(extracted.values[0].to_string() == "move") {
					result.hook_delete = true;
				} else {
					err_out.add(std::string("unknown parameter \"") + extracted.values[0].to_string() + "\" passed to hook on line "
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else if(kstr == "property") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"property\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.properties.push_back(
						parse_property_def(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing relationship defintion on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

std::vector<std::string> parse_legacy_types(char const * start, char const * end, char const * global_start, error_record & err_out) {
	std::vector<std::string> result;

	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "name") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"name\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					result.push_back(extracted.values[0].to_string());
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing legacy types list on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

conversion_def parse_conversion_def(char const * start, char const * end, char const * global_start, error_record & err_out) {
	conversion_def result;
	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr = extracted.key.to_string();
			if(kstr == "from") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"from\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(result.from.length() == 0) {
					result.from = extracted.values[0].to_string();
				} else {
					err_out.add(std::string("multiple defintion of \"from\" while parsing conversion defintion on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else if(kstr == "to") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"to\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else if(result.to.length() == 0) {
					result.to = extracted.values[0].to_string();
				} else {
					err_out.add(std::string("multiple defintion of \"to\" while parsing conversion defintion on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				}
			} else {
				err_out.add(std::string("unexpected token \"") + kstr + "\" while parsing conversion defintion on line "
					+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
			}
		}
	}
	return result;
}

file_def parse_file(char const * start, char const * end, error_record & err_out) {
	file_def parsed_file;

	char const* pos = start;
	while(pos < end) {
		auto extracted = extract_item(pos, end, start, err_out);
		pos = extracted.terminal;

		if(extracted.key.start != extracted.key.end) {
			std::string kstr(extracted.key.start, extracted.key.end);
			if(kstr == "namespace") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"namespace\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					parsed_file.namspace = extracted.values[0].to_string();
				}
			} else if(kstr == "include") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"include\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					parsed_file.includes.push_back(extracted.values[0].to_string());
				}
			} else if(kstr == "convert") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"convert\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					parsed_file.conversion_list.push_back(
						parse_conversion_def(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			} else if(kstr == "legacy_types") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"legacy_types\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					auto nlist = parse_legacy_types(extracted.values[0].start, extracted.values[0].end, start, err_out);
					for(auto& s : nlist) {
						parsed_file.legacy_types.push_back(s);
					}
				}
			} else if(kstr == "global") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"global\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					parsed_file.globals.push_back(extracted.values[0].to_string());
				}
			} else if(kstr == "load_save") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"load_save\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					parsed_file.load_save_routines.push_back(
						parse_load_save_def(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			} else if(kstr == "object") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"object\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					parsed_file.relationship_objects.push_back(
						parse_object(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			} else if(kstr == "relationship") {
				if(extracted.values.size() != 1) {
					err_out.add(std::string("wrong number of parameters for \"relationship\" on line ")
						+ std::to_string(calculate_line_from_position(start, extracted.key.start)));
				} else {
					parsed_file.relationship_objects.push_back(
						parse_relationship(extracted.values[0].start, extracted.values[0].end, start, err_out));
				}
			}
		}
	}

	return parsed_file;
}