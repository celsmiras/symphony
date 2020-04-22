package symphony.bm.cache.rules.vo.actions;

import com.fasterxml.jackson.annotation.JsonAutoDetect;
import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.AllArgsConstructor;
import lombok.Setter;
import lombok.Value;
import lombok.experimental.NonFinal;
import org.springframework.data.mongodb.core.mapping.Field;

import java.util.HashMap;

@Value
@JsonAutoDetect(fieldVisibility = JsonAutoDetect.Visibility.ANY, getterVisibility = JsonAutoDetect.Visibility.NONE,
        setterVisibility = JsonAutoDetect.Visibility.NONE)
public class Action {
    @Field("CID") @JsonProperty("CID") String CID;
    int index;
    @Setter @NonFinal String value;
    
    @JsonCreator
    public Action(@JsonProperty("CID") String CID, @JsonProperty("index") int index,
                  @JsonProperty("value") String value) {
        this.CID = CID;
        this.index = index;
        this.value = value;
    }
}
