package org.ec535.dmgturret;

import java.util.ArrayList;
import java.util.HashMap;

public class VoiceCommand {

    public final static int MAX_TICK = 10;

    public enum CommandName {
        FIRE ((byte) 0),
        PRIME ((byte) 1),
        TILT_UP ((byte) 2),
        TILT_DOWN ((byte) 3),
        ROTATE_LEFT ((byte) 4),
        ROTATE_RIGHT ((byte) 5),
        INVALID ((byte) 6);

        private byte mValue;

        CommandName(byte value) {
            mValue = value;
        }

        byte getId() {
            return mValue;
        }
    }

    private static final HashMap numberMap = new HashMap<String, Integer>() {{
        put("one", 1);
        put("two", 2);
        put("three", 3);
        put("four", 4);
        put("five", 5);
        put("six", 6);
        put("seven", 7);
        put("eight", 8);
        put("nine", 9);
        put("ten", 10);
    }};


    private static class CommandOp {
        private CommandName mCommandName;
        private boolean mIsValid;

        // this op location within the sentence
        private int mWordIndex;

        public CommandOp() {
            // Null constructor
            mWordIndex = 0;
            mCommandName = CommandName.INVALID;
            mIsValid = false;
        }

        public CommandOp(CommandName commandName, int wordIndex) {
            mWordIndex = wordIndex;
            mCommandName = commandName;
            mIsValid = true;
        }

        public CommandName getCommandName() {
            return mCommandName;
        }

        public int getWordIndex() {
            return mWordIndex;
        }

        public boolean isValid() {
            return mIsValid;
        }

        public boolean shouldHaveAnArgument() {
            return mCommandName != CommandName.FIRE
                    && mCommandName != CommandName.PRIME;
        }

    }

    private static class CommandArgument {
        private int mArgument;
        // this argument's index within the word list
        private int mWordIndex;
        private boolean isValid;


        public CommandArgument() {
            // Null constructor
            mArgument = 0;
            mWordIndex = 0;
            isValid = false;
        }

        public CommandArgument(int argument, int wordIndex) {
            mArgument = argument;
            mWordIndex = wordIndex;
            isValid = true;
        }

        public int getArgument() {
            return mArgument;
        }

        public boolean isValid() {
            return isValid;
        }
    }

    private CommandOp mCommandOp;
    private CommandArgument mArgument;
    private boolean isValid;

    protected VoiceCommand() {
        // Null voice command
        mArgument = new CommandArgument();
        mCommandOp = new CommandOp();
        isValid = true;
    }

    protected VoiceCommand(CommandOp op, CommandArgument arg) {
        mCommandOp = op;
        mArgument = arg;
        isValid = false;
    }

    public boolean hasArgument() {
        return mCommandOp.shouldHaveAnArgument();
    }

    public boolean isValid() {
        return isValid;
    }

    public int getArgument() {
        return mArgument.getArgument();
    }

    public CommandName getCommandName() {
        return mCommandOp.getCommandName();
    }

    public byte[] toBytes() {
        byte[] output = new byte[2];
        output[0] = mCommandOp.getCommandName().getId();
        output[1] = (byte) mArgument.getArgument();
        return output;
    }

    public static CommandOp getCommandOp(ArrayList<String> stringList) {
        if (stringList.size() <= 0)
            return new CommandOp();
        for (int wordIndex = 0; wordIndex < stringList.size(); wordIndex++) {
            if (stringList.get(wordIndex).toLowerCase().contains("fire"))
                return new CommandOp(CommandName.FIRE, wordIndex);
            else if (stringList.get(wordIndex).toLowerCase().contains("prime"))
                return new CommandOp(CommandName.PRIME, wordIndex);
            else if (stringList.get(wordIndex).toLowerCase().contains("up"))
                return new CommandOp(CommandName.TILT_UP, wordIndex);
            else if (stringList.get(wordIndex).toLowerCase().contains("down"))
                return new CommandOp(CommandName.TILT_DOWN, wordIndex);
            else if (stringList.get(wordIndex).toLowerCase().contains("left"))
                return new CommandOp(CommandName.ROTATE_LEFT, wordIndex);
            else if (stringList.get(wordIndex).toLowerCase().contains("right") ||
                    stringList.get(wordIndex).toLowerCase().contains("write") ||
                    stringList.get(wordIndex).toLowerCase().contains("rite"))
                return new CommandOp(CommandName.ROTATE_RIGHT, wordIndex);
        }
        return new CommandOp();
    }

    public static CommandArgument getCommandArgument(
            ArrayList<String> stringList, int beginIndex) {
        int wordIndex = 0;
        int argument = -1;
        // should we care about units?
        if (stringList.size() <= 0)
            return new CommandArgument();
        for (wordIndex = beginIndex; wordIndex < stringList.size(); wordIndex++) {
            try {
                argument = Integer.parseInt(stringList.get(wordIndex));
                if (argument <= 0 || argument > MAX_TICK)
                    argument = -1;
                break;
            } catch (NumberFormatException ignored) {
                if (numberMap.containsKey(stringList.get(wordIndex))) {
                    argument = (int)numberMap.get(stringList.get(wordIndex));
                    break;
                }
            }
        }
        if (argument >= 0)
            return new CommandArgument(argument, wordIndex);
        else
            return new CommandArgument();
    }

    public static VoiceCommand getCommand(ArrayList<String> stringList) {
        CommandOp op = VoiceCommand.getCommandOp(stringList);
        CommandArgument arg = VoiceCommand.getCommandArgument(
                stringList, op.getWordIndex());
        if (!op.isValid() || (op.shouldHaveAnArgument() && !arg.isValid())) {
            return new VoiceCommand();
        } else {
            return new VoiceCommand(op, arg);
        }
    }
}